/*
 * Kindle backend for Kindlebrew Game Engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#ifdef KB_KINDLE

#include "fbink.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define KB_MAX_INPUT_DEVICES 16
#define KB_MAX_TOUCH_SLOTS 16

typedef struct {
    bool active;
    bool prev_active;
    bool changed;
    int raw_x;
    int raw_y;
    int prev_raw_x;
    int prev_raw_y;
    int tracking_id;
} KBTouchSlot;

typedef struct {
    int fd;
    uint32_t type;
    bool grabbed;
    bool has_mt;
    bool has_abs;
    int slot;
    struct input_absinfo xinfo;
    struct input_absinfo yinfo;
    KBTouchSlot slots[KB_MAX_TOUCH_SLOTS];
} KBInputDev;

typedef struct {
    int fbfd;
    int lockfd;
    FBInkConfig fb_cfg;
    FBInkState fb_state;
    unsigned char *fb;
    size_t fb_size;
    bool direct_y8;
    bool keep_awake_set;
    bool mtk_fast_mode_set;

    int power_fd;
    pid_t power_pid;
    char power_buf[512];
    size_t power_len;
    bool suspended;
    uint64_t last_suspend_ms;
    uint64_t last_suspend_duration_ms;
    uint64_t power_retry_due_ms;

    uint64_t rotation_recheck_due_ms;
    int rotation_rechecks_left;

    KBInputDev input[KB_MAX_INPUT_DEVICES];
    int input_count;

    bool gesture_down;
    bool hold_emitted;
    int gesture_id;
    int start_x;
    int start_y;
    int last_x;
    int last_y;
    uint64_t down_ms;
    uint64_t last_tap_ms;
    int last_tap_x;
    int last_tap_y;

    struct sigaction old_sigint;
    struct sigaction old_sigterm;
    struct sigaction old_sighup;
    bool sigint_installed;
    bool sigterm_installed;
    bool sighup_installed;
} KBKindle;

static volatile sig_atomic_t kb_signal_quit = 0;

static void kb_signal_handler(int signo) {
    (void)signo;
    kb_signal_quit = 1;
}

static void install_signal_handlers(KBKindle *k) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = kb_signal_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, &k->old_sigint) == 0) k->sigint_installed = true;
    if (sigaction(SIGTERM, &sa, &k->old_sigterm) == 0) k->sigterm_installed = true;
    if (sigaction(SIGHUP, &sa, &k->old_sighup) == 0) k->sighup_installed = true;
}

static void restore_signal_handlers(KBKindle *k) {
    if (k->sigint_installed) (void)sigaction(SIGINT, &k->old_sigint, NULL);
    if (k->sigterm_installed) (void)sigaction(SIGTERM, &k->old_sigterm, NULL);
    if (k->sighup_installed) (void)sigaction(SIGHUP, &k->old_sighup, NULL);
    k->sigint_installed = k->sigterm_installed = k->sighup_installed = false;
}

static void harden_input_fd(int fd) {
    if (fd < 0) return;
    int fdflags = fcntl(fd, F_GETFD, 0);
    if (fdflags >= 0) (void)fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int run_quiet_argv(const char *file, char *const argv[]) {
    if (!file || !argv || !argv[0]) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd >= 0) {
            (void)dup2(nullfd, STDIN_FILENO);
            (void)dup2(nullfd, STDOUT_FILENO);
            (void)dup2(nullfd, STDERR_FILENO);
            if (nullfd > STDERR_FILENO) close(nullfd);
        }
        execvp(file, argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int set_prevent_screensaver(bool enabled) {
    char *argv[] = {
        (char *)"lipc-set-prop",
        (char *)"com.lab126.powerd",
        (char *)"preventScreenSaver",
        enabled ? (char *)"1" : (char *)"0",
        NULL
    };
    return run_quiet_argv("lipc-set-prop", argv);
}

static int request_native_repaint(void) {
    char *argv[] = {
        (char *)"xrefresh",
        (char *)"-d",
        (char *)":0.0",
        NULL
    };
    return run_quiet_argv("/usr/bin/xrefresh", argv);
}

static void apply_low_latency_mode(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k || !game->config.low_latency_mode || !k->fb_state.is_mtk || k->fbfd < 0) return;

    int rc = fbink_mtk_toggle_auto_reagl(k->fbfd, false);
    if (rc == 0) {
        k->mtk_fast_mode_set = true;
        game->device.mtk_fast_mode_active = true;
    } else if (rc != -ENOSYS) {
        fprintf(stderr, "kbgame: warning: failed to enable MTK low-latency mode: %d\n", rc);
    }
}

static void update_device_from_state(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    game->device.width = game->canvas.width;
    game->device.height = game->canvas.height;
    game->device.dpi = k->fb_state.screen_dpi;
    game->device.bpp = (int)k->fb_state.bpp;
    game->device.pixel_format = (int)k->fb_state.pixel_format;
    game->device.is_mtk = k->fb_state.is_mtk;
    game->device.can_rotate = k->fb_state.can_rotate;
    game->device.can_hw_invert = k->fb_state.can_hw_invert;
    game->device.has_eclipse_waveform = k->fb_state.has_eclipse_wfm;
    game->device.has_color_panel = k->fb_state.has_color_panel;
    game->device.can_wait_for_submission = k->fb_state.can_wait_for_submission;
    game->device.direct_framebuffer_y8 = k->direct_y8;
}

static void refresh_fb_pointer(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    k->fb = fbink_get_fb_pointer(k->fbfd, &k->fb_size);
    k->direct_y8 = k->fb &&
                   k->fb_state.pixel_format == FBINK_PXFMT_Y8 &&
                   k->fb_state.bpp == 8 &&
                   !k->fb_state.inverted_grayscale &&
                   k->fb_state.view_width == k->fb_state.screen_width &&
                   k->fb_state.view_height == k->fb_state.screen_height &&
                   k->fb_state.view_hori_origin == 0 &&
                   k->fb_state.view_vert_origin == 0 &&
                   k->fb_state.current_rota == FB_ROTATE_UR;
    game->device.direct_framebuffer_y8 = k->direct_y8;
}

static int resize_canvas_for_state(KBGame *game, const FBInkState *state) {
    int width = (int)state->screen_width;
    int height = (int)state->screen_height;
    if (width <= 0 || height <= 0) return -1;
    if (width == game->canvas.width && height == game->canvas.height) return 0;

    size_t size = (size_t)width * (size_t)height;
    uint8_t *pixels = malloc(size);
    if (!pixels) {
        kb_set_error(game, "out of memory resizing canvas to %dx%d", width, height);
        return -1;
    }
    memset(pixels, game->config.background, size);
    free(game->canvas.pixels);
    game->canvas.pixels = pixels;
    game->canvas.width = width;
    game->canvas.height = height;
    game->canvas.stride = width;

    KBEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = KB_EVENT_RESIZE;
    ev.time_ms = kb_now_ms();
    ev.width = width;
    ev.height = height;
    kb_event_push(game, &ev);
    return 1;
}

static int revalidate_framebuffer(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    int rc = fbink_reinit(k->fbfd, &k->fb_cfg);
    if (rc < 0) {
        kb_set_error(game, "fbink_reinit failed: %d", rc);
        return -1;
    }
    if (rc == 0) return 0;

    FBInkState state;
    memset(&state, 0, sizeof(state));
    fbink_get_state(&k->fb_cfg, &state);

    int resized = resize_canvas_for_state(game, &state);
    if (resized < 0) return -1;

    k->fb_state = state;
    refresh_fb_pointer(game);
    update_device_from_state(game);
    apply_low_latency_mode(game);

    kb_damage_reset(game);
    kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height}, false);
    kb_policy_reset(game, kb_now_ms());
    return resized ? 2 : 1;
}

static int redraw_after_resume(KBGame *game) {
    int rc = revalidate_framebuffer(game);
    if (rc < 0) return -1;

    /*
     * A layout change invalidates the old coordinate system. The RESIZE event
     * is already queued; let the game lay itself out before anything is
     * presented instead of flashing a blank/stretched intermediate frame.
     */
    if (rc == 2) return 0;

    kb_damage_reset(game);
    kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height}, false);
    kb_policy_reset(game, kb_now_ms());
    return kb_present(game, KB_REFRESH_CLEAN);
}

static int parse_event_int(const char *line, const char *name) {
    const char *p = line + strlen(name);
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;
    if (v > INT_MAX) return INT_MAX;
    if (v < INT_MIN) return INT_MIN;
    return (int)v;
}

static int setup_power_events(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    int p[2];
    if (pipe(p) != 0) {
        fprintf(stderr, "kbgame: warning: pipe(powerd): %s\n", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        fprintf(stderr, "kbgame: warning: fork(powerd): %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        close(p[0]);
        if (dup2(p[1], STDOUT_FILENO) < 0) _exit(126);
        close(p[1]);

        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            (void)dup2(nullfd, STDERR_FILENO);
            if (nullfd != STDERR_FILENO) close(nullfd);
        }

        execlp("lipc-wait-event", "lipc-wait-event",
               "-m", "-s", "0", "com.lab126.powerd",
               "goingToScreenSaver,outOfScreenSaver,exitingScreenSaver,charging,notCharging,wakeupFromSuspend,readyToSuspend",
               (char *)NULL);
        _exit(127);
    }

    close(p[1]);
    int flags = fcntl(p[0], F_GETFL, 0);
    if (flags >= 0) (void)fcntl(p[0], F_SETFL, flags | O_NONBLOCK);
    (void)fcntl(p[0], F_SETFD, FD_CLOEXEC);
    k->power_fd = p[0];
    k->power_pid = pid;
    k->power_len = 0;
    return 0;
}

static void sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

static void teardown_power_events(KBKindle *k) {
    if (k->power_fd >= 0) {
        close(k->power_fd);
        k->power_fd = -1;
    }
    if (k->power_pid > 0) {
        kill(k->power_pid, SIGINT);
        for (int i = 0; i < 20; ++i) {
            pid_t rc = waitpid(k->power_pid, NULL, WNOHANG);
            if (rc == k->power_pid || rc < 0) {
                k->power_pid = 0;
                return;
            }
            sleep_ms(10);
        }
        kill(k->power_pid, SIGTERM);
        (void)waitpid(k->power_pid, NULL, 0);
        k->power_pid = 0;
    }
}

static void reset_touch_state(KBKindle *k) {
    k->gesture_down = false;
    k->hold_emitted = false;
    for (int d = 0; d < k->input_count; ++d) {
        for (int s = 0; s < KB_MAX_TOUCH_SLOTS; ++s) {
            memset(&k->input[d].slots[s], 0, sizeof(k->input[d].slots[s]));
        }
    }
}

static void handle_power_line(KBGame *game, const char *line) {
    KBKindle *k = (KBKindle *)game->backend;
    uint64_t now = kb_now_ms();
    KBEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.time_ms = now;

    if (strncmp(line, "goingToScreenSaver", 18) == 0) {
        int source = parse_event_int(line, "goingToScreenSaver");
        if (!k->suspended) {
            k->suspended = true;
            k->last_suspend_ms = now;
            reset_touch_state(k);
            ev.type = KB_EVENT_SUSPEND;
            ev.source = source;
            kb_event_push(game, &ev);
        }
        return;
    }

    if (strncmp(line, "readyToSuspend", 14) == 0) {
        int delay = parse_event_int(line, "readyToSuspend");
        if (!k->suspended) {
            /*
             * Normally goingToScreenSaver arrived first. Treat ReadyToSuspend
             * as a final fallback checkpoint on firmware paths that skipped
             * the screensaver notification, but never emit duplicate SUSPENDs.
             */
            k->suspended = true;
            k->last_suspend_ms = now;
            reset_touch_state(k);
            ev.type = KB_EVENT_SUSPEND;
            ev.source = 2; /* readyToSuspend fallback */
            ev.value = delay;
            kb_event_push(game, &ev);
        }
        return;
    }

    if (strncmp(line, "wakeupFromSuspend", 17) == 0) {
        int raw = parse_event_int(line, "wakeupFromSuspend");
        if (raw > 0) k->last_suspend_duration_ms = (uint64_t)(unsigned)raw * 1000ULL;
        return;
    }

    if (strncmp(line, "outOfScreenSaver", 16) == 0) {
        int source = parse_event_int(line, "outOfScreenSaver");
        if (redraw_after_resume(game) != 0) {
            fprintf(stderr, "kbgame: warning: resume redraw failed: %s\n", kb_last_error(game));
        }
        k->suspended = false;
        ev.type = KB_EVENT_RESUME;
        ev.source = source;
        ev.duration_ms = k->last_suspend_duration_ms ? k->last_suspend_duration_ms :
                         (k->last_suspend_ms && now >= k->last_suspend_ms ? now - k->last_suspend_ms : 0);
        k->last_suspend_duration_ms = 0;
        kb_event_push(game, &ev);
        return;
    }
}

static void drain_power_events(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    char buf[256];
    ssize_t n;

    while ((n = read(k->power_fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == '\n') {
                k->power_buf[k->power_len] = '\0';
                if (k->power_len) handle_power_line(game, k->power_buf);
                k->power_len = 0;
            } else if (ch != '\r') {
                if (k->power_len + 1U < sizeof(k->power_buf)) {
                    k->power_buf[k->power_len++] = ch;
                } else {
                    k->power_len = 0;
                }
            }
        }
    }
}

static int absinfo_for(int fd, int primary, int fallback, struct input_absinfo *out) {
    if (ioctl(fd, EVIOCGABS(primary), out) == 0 && out->maximum > out->minimum) return primary;
    memset(out, 0, sizeof(*out));
    if (ioctl(fd, EVIOCGABS(fallback), out) == 0 && out->maximum > out->minimum) return fallback;
    memset(out, 0, sizeof(*out));
    out->minimum = 0;
    out->maximum = 4095;
    return -1;
}

static int norm_axis(int raw, const struct input_absinfo *a) {
    int64_t min = a->minimum;
    int64_t max = a->maximum;
    if (max <= min) return 0;
    int64_t v = raw;
    if (v < min) v = min;
    if (v > max) v = max;
    return (int)(((v - min) * 65535LL) / (max - min));
}

static void map_touch(KBGame *game, const KBInputDev *dev, int raw_x, int raw_y, int *x, int *y) {
    KBKindle *k = (KBKindle *)game->backend;
    int nx = norm_axis(raw_x, &dev->xinfo);
    int ny = norm_axis(raw_y, &dev->yinfo);

    /*
     * FBInk's device quirks describe the panel's physical reporting oddities.
     * Normalize first, then apply them; this avoids coupling to raw axis ranges.
     */
    if (k->fb_state.touch_swap_axes) {
        int t = nx; nx = ny; ny = t;
    }
    if (k->fb_state.touch_mirror_x) nx = 65535 - nx;
    if (k->fb_state.touch_mirror_y) ny = 65535 - ny;

    *x = game->canvas.width > 1 ? (int)(((int64_t)nx * (game->canvas.width - 1)) / 65535) : 0;
    *y = game->canvas.height > 1 ? (int)(((int64_t)ny * (game->canvas.height - 1)) / 65535) : 0;
}

static unsigned default_tap_slop(const KBGame *game) {
    if (game->config.tap_slop_px) return game->config.tap_slop_px;
    unsigned v = game->device.dpi > 0 ? (unsigned)game->device.dpi / 12U : 20U;
    return v < 12U ? 12U : v;
}

static unsigned default_swipe_min(const KBGame *game) {
    if (game->config.swipe_min_px) return game->config.swipe_min_px;
    unsigned v = game->device.dpi > 0 ? (unsigned)game->device.dpi / 4U : 60U;
    return v < 40U ? 40U : v;
}

static int dist_chebyshev(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    return dx > dy ? dx : dy;
}

static void emit_touch(KBGame *game, KBEventType type, int id, int x, int y, uint64_t now) {
    KBEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.time_ms = now;
    ev.id = id;
    ev.x = x;
    ev.y = y;
    kb_event_push(game, &ev);
}

static void gesture_down(KBGame *game, int id, int x, int y, uint64_t now) {
    KBKindle *k = (KBKindle *)game->backend;
    if (k->gesture_down) return;
    k->gesture_down = true;
    k->hold_emitted = false;
    k->gesture_id = id;
    k->start_x = k->last_x = x;
    k->start_y = k->last_y = y;
    k->down_ms = now;
}

static void gesture_move(KBGame *game, int id, int x, int y) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k->gesture_down || k->gesture_id != id) return;
    k->last_x = x;
    k->last_y = y;
}

static void gesture_up(KBGame *game, int id, int x, int y, uint64_t now) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k->gesture_down || k->gesture_id != id) return;

    k->last_x = x;
    k->last_y = y;
    int distance = dist_chebyshev(k->start_x, k->start_y, x, y);
    uint64_t elapsed = now - k->down_ms;
    unsigned swipe_min = default_swipe_min(game);
    unsigned slop = default_tap_slop(game);

    KBEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.time_ms = now;
    ev.id = id;
    ev.x = x;
    ev.y = y;
    ev.start_x = k->start_x;
    ev.start_y = k->start_y;
    ev.dx = x - k->start_x;
    ev.dy = y - k->start_y;

    if ((unsigned)distance >= swipe_min) {
        ev.type = KB_EVENT_SWIPE;
        kb_event_push(game, &ev);
    } else if (!k->hold_emitted && (unsigned)distance <= slop &&
               elapsed <= (game->config.tap_timeout_ms ? game->config.tap_timeout_ms : 350U)) {
        unsigned double_ms = game->config.double_tap_ms ? game->config.double_tap_ms : 450U;
        bool double_tap = k->last_tap_ms &&
                          now - k->last_tap_ms <= double_ms &&
                          (unsigned)dist_chebyshev(k->last_tap_x, k->last_tap_y, x, y) <= slop * 2U;
        ev.type = double_tap ? KB_EVENT_DOUBLE_TAP : KB_EVENT_TAP;
        kb_event_push(game, &ev);
        if (double_tap) {
            k->last_tap_ms = 0;
        } else {
            k->last_tap_ms = now;
            k->last_tap_x = x;
            k->last_tap_y = y;
        }
    }

    k->gesture_down = false;
    k->hold_emitted = false;
}

static void maybe_emit_hold(KBGame *game, uint64_t now) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k->gesture_down || k->hold_emitted) return;
    unsigned hold = game->config.hold_ms ? game->config.hold_ms : 650U;
    if (now - k->down_ms < hold) return;
    if ((unsigned)dist_chebyshev(k->start_x, k->start_y, k->last_x, k->last_y) > default_tap_slop(game)) return;

    KBEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = KB_EVENT_HOLD;
    ev.time_ms = now;
    ev.id = k->gesture_id;
    ev.x = k->last_x;
    ev.y = k->last_y;
    ev.start_x = k->start_x;
    ev.start_y = k->start_y;
    kb_event_push(game, &ev);
    k->hold_emitted = true;
}

static void process_syn(KBGame *game, KBInputDev *dev) {
    uint64_t now = kb_now_ms();
    for (int i = 0; i < KB_MAX_TOUCH_SLOTS; ++i) {
        KBTouchSlot *s = &dev->slots[i];
        if (!s->active && !s->prev_active && !s->changed) continue;

        int x = 0, y = 0;
        int rx = s->active ? s->raw_x : s->prev_raw_x;
        int ry = s->active ? s->raw_y : s->prev_raw_y;
        map_touch(game, dev, rx, ry, &x, &y);

        if (!s->prev_active && s->active) {
            emit_touch(game, KB_EVENT_TOUCH_DOWN, i, x, y, now);
            gesture_down(game, i, x, y, now);
        } else if (s->prev_active && s->active && s->changed) {
            emit_touch(game, KB_EVENT_TOUCH_MOVE, i, x, y, now);
            gesture_move(game, i, x, y);
        } else if (s->prev_active && !s->active) {
            emit_touch(game, KB_EVENT_TOUCH_UP, i, x, y, now);
            gesture_up(game, i, x, y, now);
        }

        s->prev_active = s->active;
        s->prev_raw_x = s->raw_x;
        s->prev_raw_y = s->raw_y;
        s->changed = false;
    }
}

static void process_input_event(KBGame *game, KBInputDev *dev, const struct input_event *ie) {
    int slot = dev->slot;
    if (slot < 0 || slot >= KB_MAX_TOUCH_SLOTS) slot = 0;
    KBTouchSlot *s = &dev->slots[slot];

    if (ie->type == EV_ABS) {
        if ((dev->type & INPUT_ROTATION_EVENT) && ie->code == ABS_PRESSURE) {
            /*
             * Kindle accelerometer codes are not globally stable. The older
             * Oasis mapping groups 17/18 with portrait variants, while newer
             * Oasis/Scribe kernels use 17/18 for CW/CCW landscape. Preserve
             * the raw kernel value and only publish degrees for codes whose
             * meaning is unambiguous across both known mappings.
             */
            int orientation = -1;
            switch (ie->value) {
                case 15: case 19: orientation = 0; break;
                case 16: case 20: orientation = 180; break;
                case 21: orientation = 90; break;
                case 22: orientation = 270; break;
                case 17: case 18: orientation = -1; break;
                default: return;
            }

            KBEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KB_EVENT_ORIENTATION;
            ev.time_ms = kb_now_ms();
            ev.orientation = orientation;
            ev.value = ie->value;
            kb_event_push(game, &ev);

            KBKindle *k = (KBKindle *)game->backend;
            k->rotation_recheck_due_ms = ev.time_ms + 150U;
            k->rotation_rechecks_left = 3;
            return;
        }

        switch (ie->code) {
            case ABS_MT_SLOT:
                if (ie->value >= 0 && ie->value < KB_MAX_TOUCH_SLOTS) dev->slot = ie->value;
                return;
            case ABS_MT_TRACKING_ID:
                s = &dev->slots[dev->slot >= 0 && dev->slot < KB_MAX_TOUCH_SLOTS ? dev->slot : 0];
                if (ie->value < 0) {
                    s->active = false;
                } else {
                    s->active = true;
                    s->tracking_id = ie->value;
                }
                s->changed = true;
                return;
            case ABS_MT_POSITION_X:
            case ABS_X:
                s->raw_x = ie->value;
                s->changed = true;
                return;
            case ABS_MT_POSITION_Y:
            case ABS_Y:
                s->raw_y = ie->value;
                s->changed = true;
                return;
            default:
                return;
        }
    }

    if (ie->type == EV_KEY) {
        if (ie->code == BTN_TOUCH &&
            (dev->type & (INPUT_TOUCHSCREEN | INPUT_SCALED_TABLET | INPUT_TABLET))) {
            if (ie->value == 0) {
                /*
                 * Some Kindle touch protocols omit MT_TRACKING_ID=-1 on lift.
                 * BTN_TOUCH=0 is authoritative that contact ended; closing all
                 * active slots is also harmless on normal protocol-B devices.
                 */
                for (int i = 0; i < KB_MAX_TOUCH_SLOTS; ++i) {
                    if (dev->slots[i].active || dev->slots[i].prev_active) {
                        dev->slots[i].active = false;
                        dev->slots[i].changed = true;
                    }
                }
            } else if (!dev->has_mt) {
                KBTouchSlot *p = &dev->slots[0];
                p->active = true;
                p->changed = true;
            }
            return;
        }

        /* BTN_* belongs to touch/stylus semantics, not game keyboard controls. */
        if (ie->code < BTN_MISC) {
            KBEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KB_EVENT_KEY;
            ev.time_ms = kb_now_ms();
            ev.key = ie->code;
            ev.value = ie->value;
            kb_event_push(game, &ev);
        }
        return;
    }

    if (ie->type == EV_SYN && ie->code == SYN_REPORT) process_syn(game, dev);
}

static void close_unclaimed_scan_fds(FBInkInputDevice *scan, size_t count) {
    if (!scan) return;
    for (size_t i = 0; i < count; ++i) {
        if (scan[i].fd >= 0) {
            close(scan[i].fd);
            scan[i].fd = -1;
        }
    }
}

static int setup_input(KBGame *game) {
    KBKindle *k = (KBKindle *)game->backend;
    size_t count = 0;
    INPUT_DEVICE_TYPE_T match = INPUT_TOUCHSCREEN | INPUT_SCALED_TABLET |
                                INPUT_PAGINATION_BUTTONS | INPUT_HOME_BUTTON |
                                INPUT_DPAD | INPUT_MENU_BUTTON | INPUT_KINDLE_FRAME_TAP |
                                INPUT_POWER_BUTTON;

    FBInkInputDevice *scan = fbink_input_scan(match, 0, NO_RECAP, &count);
    if (!scan) {
        kb_set_error(game, "FBInk could not discover Kindle input devices");
        return -1;
    }

    for (size_t i = 0; i < count && k->input_count < KB_MAX_INPUT_DEVICES; ++i) {
        if (!scan[i].matched || scan[i].fd < 0) continue;
        KBInputDev *d = &k->input[k->input_count++];
        memset(d, 0, sizeof(*d));
        d->fd = scan[i].fd;
        scan[i].fd = -1; /* ownership transferred to KBKindle */
        harden_input_fd(d->fd);
        d->type = scan[i].type;
        d->slot = 0;

        bool touch = (d->type & (INPUT_TOUCHSCREEN | INPUT_SCALED_TABLET | INPUT_TABLET)) != 0;
        if (touch) {
            int xcode = absinfo_for(d->fd, ABS_MT_POSITION_X, ABS_X, &d->xinfo);
            int ycode = absinfo_for(d->fd, ABS_MT_POSITION_Y, ABS_Y, &d->yinfo);
            d->has_mt = xcode == ABS_MT_POSITION_X && ycode == ABS_MT_POSITION_Y;
            d->has_abs = true;
            if (game->config.grab_touch) {
                if (ioctl(d->fd, EVIOCGRAB, 1) == 0) {
                    d->grabbed = true;
                } else {
                    fprintf(stderr, "kbgame: warning: EVIOCGRAB failed on touch fd %d: %s\n",
                            d->fd, strerror(errno));
                }
            }
        }
    }

    /*
     * FBInk closes unmatched fds itself. Close any still-open matched fd that
     * we did not adopt (e.g., because the fixed input table hit capacity).
     */
    close_unclaimed_scan_fds(scan, count);
    free(scan);

    /* Gyro/rotation events are scanned separately to avoid ABS_PRESSURE false positives on touch/tablet devices. */
    size_t rot_count = 0;
    FBInkInputDevice *rot = fbink_input_scan(INPUT_ROTATION_EVENT,
                                             INPUT_TABLET | INPUT_TOUCHSCREEN,
                                             NO_RECAP, &rot_count);
    if (rot) {
        for (size_t i = 0; i < rot_count && k->input_count < KB_MAX_INPUT_DEVICES; ++i) {
            if (!rot[i].matched || rot[i].fd < 0) continue;
            KBInputDev *d = &k->input[k->input_count++];
            memset(d, 0, sizeof(*d));
            d->fd = rot[i].fd;
            rot[i].fd = -1; /* ownership transferred to KBKindle */
            harden_input_fd(d->fd);
            d->type = rot[i].type;
            d->slot = 0;
        }
        close_unclaimed_scan_fds(rot, rot_count);
        free(rot);
    }

    if (k->input_count == 0) {
        kb_set_error(game, "no usable Kindle input devices found");
        return -1;
    }
    return 0;
}

static void teardown_input(KBKindle *k) {
    for (int i = 0; i < k->input_count; ++i) {
        if (k->input[i].fd < 0) continue;
        if (k->input[i].grabbed) ioctl(k->input[i].fd, EVIOCGRAB, 0);
        close(k->input[i].fd);
        k->input[i].fd = -1;
    }
    k->input_count = 0;
}

static WFM_MODE_INDEX_T waveform(KBRefreshMode mode) {
    switch (mode) {
        case KB_REFRESH_FAST_MONO: return WFM_A2;
        case KB_REFRESH_UI:        return WFM_DU;
        case KB_REFRESH_TEXT:      return WFM_GL16;
        case KB_REFRESH_GRAY:      return WFM_GC16;
        case KB_REFRESH_CLEAN:     return WFM_GC16;
        case KB_REFRESH_AUTO:
        default:                   return WFM_AUTO;
    }
}

static int copy_rect_direct_y8(KBGame *game, const KBRect *r) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k->fb || !k->direct_y8) return -1;
    for (int row = 0; row < r->h; ++row) {
        size_t dst_off = (size_t)(r->y + row) * k->fb_state.scanline_stride + (size_t)r->x;
        size_t src_off = (size_t)(r->y + row) * game->canvas.stride + (size_t)r->x;
        if (dst_off + (size_t)r->w > k->fb_size) return -1;
        memcpy(k->fb + dst_off, game->canvas.pixels + src_off, (size_t)r->w);
    }
    return 0;
}

static int copy_rect_via_fbink(KBGame *game, const KBRect *r) {
    KBKindle *k = (KBKindle *)game->backend;
    size_t n = (size_t)r->w * (size_t)r->h;
    uint8_t *tmp = malloc(n);
    if (!tmp) {
        kb_set_error(game, "out of memory allocating %zu-byte FBInk staging buffer", n);
        return -1;
    }

    for (int row = 0; row < r->h; ++row) {
        memcpy(tmp + (size_t)row * r->w,
               game->canvas.pixels + (size_t)(r->y + row) * game->canvas.stride + r->x,
               (size_t)r->w);
    }

    FBInkConfig draw = k->fb_cfg;
    draw.no_refresh = true;
    draw.ignore_alpha = true;
    draw.is_flashing = false;
    int rc = fbink_print_raw_data(k->fbfd, tmp, r->w, r->h, n, (short)r->x, (short)r->y, &draw);
    free(tmp);
    if (rc < 0) {
        kb_set_error(game, "fbink_print_raw_data failed: %d", rc);
        return -1;
    }
    return 0;
}

static int kindle_present(KBGame *game, const KBRect *rects, int count, KBRefreshMode mode, bool flashing) {
    KBKindle *k = (KBKindle *)game->backend;
    if (!k || k->fbfd < 0) return -1;

    if (flashing && !k->fb_state.unreliable_wait_for) {
        /* Fence the previous batch before a disruptive full waveform. */
        (void)fbink_wait_for_complete(k->fbfd, LAST_MARKER);
    }

    for (int i = 0; i < count; ++i) {
        KBRect r = kb_rect_clip(rects[i], game->canvas.width, game->canvas.height);
        if (kb_rect_empty(r)) continue;

        if (copy_rect_direct_y8(game, &r) != 0 && copy_rect_via_fbink(game, &r) != 0) return -1;
    }

    FBInkConfig refresh = k->fb_cfg;
    refresh.no_refresh = false;
    refresh.wfm_mode = waveform(mode);
    refresh.is_flashing = flashing;

    for (int i = 0; i < count; ++i) {
        KBRect r = kb_rect_clip(rects[i], game->canvas.width, game->canvas.height);
        if (kb_rect_empty(r)) continue;
        FBInkRect fr = {(unsigned short)r.x, (unsigned short)r.y,
                        (unsigned short)r.w, (unsigned short)r.h};
        int rc = fbink_refresh_rect(k->fbfd, &fr, &refresh);
        if (rc < 0) {
            kb_set_error(game, "fbink_refresh_rect failed: %d", rc);
            return -1;
        }
    }
    return 0;
}

static int kindle_poll(KBGame *game, KBEvent *event, int timeout_ms) {
    KBKindle *k = (KBKindle *)game->backend;
    if (kb_event_pop(game, event)) return 1;

    if (kb_signal_quit) {
        memset(event, 0, sizeof(*event));
        event->type = KB_EVENT_QUIT;
        event->time_ms = kb_now_ms();
        return 1;
    }

    maybe_emit_hold(game, kb_now_ms());
    if (kb_event_pop(game, event)) return 1;

    struct pollfd pfds[KB_MAX_INPUT_DEVICES + 1];
    int map[KB_MAX_INPUT_DEVICES + 1];
    int n = 0;
    for (int i = 0; i < k->input_count; ++i) {
        if (k->input[i].fd < 0) continue;
        pfds[n].fd = k->input[i].fd;
        pfds[n].events = POLLIN;
        pfds[n].revents = 0;
        map[n] = i;
        ++n;
    }
    if (k->power_fd >= 0) {
        pfds[n].fd = k->power_fd;
        pfds[n].events = POLLIN;
        pfds[n].revents = 0;
        map[n] = -1;
        ++n;
    }
    if (n == 0) return 0;

    int effective_timeout = timeout_ms;
    if (k->gesture_down && !k->hold_emitted) {
        unsigned hold = game->config.hold_ms ? game->config.hold_ms : 650U;
        uint64_t now = kb_now_ms();
        uint64_t due = k->down_ms + hold;
        int until_hold = due > now ? (int)(due - now) : 0;
        if (effective_timeout < 0 || until_hold < effective_timeout) effective_timeout = until_hold;
    }
    if (k->rotation_rechecks_left > 0 && k->rotation_recheck_due_ms) {
        uint64_t now = kb_now_ms();
        int until_recheck = k->rotation_recheck_due_ms > now ?
                            (int)(k->rotation_recheck_due_ms - now) : 0;
        if (effective_timeout < 0 || until_recheck < effective_timeout) effective_timeout = until_recheck;
    }
    if (k->power_fd < 0 && k->power_retry_due_ms) {
        uint64_t now = kb_now_ms();
        int until_retry = k->power_retry_due_ms > now ?
                          (int)(k->power_retry_due_ms - now) : 0;
        if (effective_timeout < 0 || until_retry < effective_timeout) effective_timeout = until_retry;
    }

    int rc = poll(pfds, (nfds_t)n, effective_timeout);
    if (rc < 0) {
        if (errno == EINTR) return kindle_poll(game, event, 0);
        kb_set_error(game, "poll(input) failed: %s", strerror(errno));
        return -1;
    }

    if (rc > 0) {
        for (int p = 0; p < n; ++p) {
            if (map[p] < 0) {
                if (pfds[p].revents & POLLIN) drain_power_events(game);
                if (pfds[p].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    teardown_power_events(k);
                    k->power_retry_due_ms = kb_now_ms() + 1000U;
                }
                continue;
            }
            if (!(pfds[p].revents & POLLIN)) continue;
            KBInputDev *d = &k->input[map[p]];
            struct input_event buf[32];
            ssize_t got;
            while ((got = read(d->fd, buf, sizeof(buf))) > 0) {
                size_t count = (size_t)got / sizeof(buf[0]);
                for (size_t j = 0; j < count; ++j) process_input_event(game, d, &buf[j]);
            }
            if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "kbgame: warning: read input fd %d: %s\n", d->fd, strerror(errno));
            }
        }
    }

    uint64_t now = kb_now_ms();

    if (k->power_fd < 0 && k->power_retry_due_ms && now >= k->power_retry_due_ms) {
        if (setup_power_events(game) == 0) {
            k->power_retry_due_ms = 0;
        } else {
            k->power_retry_due_ms = now + 5000U;
        }
    }

    if (k->rotation_rechecks_left > 0 && k->rotation_recheck_due_ms &&
        now >= k->rotation_recheck_due_ms) {
        int changed = revalidate_framebuffer(game);
        if (changed > 0) {
            if (changed != 2) {
                kb_damage_reset(game);
                kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height}, false);
                (void)kb_present(game, KB_REFRESH_CLEAN);
            }
            k->rotation_rechecks_left = 0;
            k->rotation_recheck_due_ms = 0;
        } else if (changed == 0 && --k->rotation_rechecks_left > 0) {
            k->rotation_recheck_due_ms = now + 150U;
        } else {
            k->rotation_recheck_due_ms = 0;
        }
    }

    maybe_emit_hold(game, now);
    return kb_event_pop(game, event) ? 1 : 0;
}

static int kindle_init(KBGame *game) {
    KBKindle *k = calloc(1, sizeof(*k));
    if (!k) return -1;
    game->backend = k;
    k->fbfd = -1;
    k->lockfd = -1;
    k->power_fd = -1;
    k->power_pid = 0;

    k->lockfd = open("/tmp/kindlebrew-game-engine.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (k->lockfd < 0 || flock(k->lockfd, LOCK_EX | LOCK_NB) != 0) {
        kb_set_error(game, "another Kindlebrew game engine instance is active");
        return -1;
    }

    memset(&k->fb_cfg, 0, sizeof(k->fb_cfg));
    k->fb_cfg.is_quiet = true;
    k->fb_cfg.ignore_alpha = true;

    k->fbfd = fbink_open();
    if (k->fbfd < 0) {
        kb_set_error(game, "fbink_open failed: %d", k->fbfd);
        return -1;
    }
    (void)fcntl(k->fbfd, F_SETFD, FD_CLOEXEC);
    int rc = fbink_init(k->fbfd, &k->fb_cfg);
    if (rc < 0) {
        kb_set_error(game, "fbink_init failed: %d", rc);
        return -1;
    }

    memset(&k->fb_state, 0, sizeof(k->fb_state));
    fbink_get_state(&k->fb_cfg, &k->fb_state);

    game->canvas.width = (int)k->fb_state.screen_width;
    game->canvas.height = (int)k->fb_state.screen_height;
    game->canvas.stride = game->canvas.width;

    if (game->config.width > 0 && game->config.width != game->canvas.width) {
        kb_set_error(game, "requested width %d does not match Kindle framebuffer width %d",
                     game->config.width, game->canvas.width);
        return -1;
    }
    if (game->config.height > 0 && game->config.height != game->canvas.height) {
        kb_set_error(game, "requested height %d does not match Kindle framebuffer height %d",
                     game->config.height, game->canvas.height);
        return -1;
    }

    memset(&game->device, 0, sizeof(game->device));
    strncpy(game->device.device_name, k->fb_state.device_name, sizeof(game->device.device_name)-1);
    strncpy(game->device.device_codename, k->fb_state.device_codename, sizeof(game->device.device_codename)-1);
    strncpy(game->device.device_platform, k->fb_state.device_platform, sizeof(game->device.device_platform)-1);
    game->device.width = game->canvas.width;
    game->device.height = game->canvas.height;
    game->device.dpi = k->fb_state.screen_dpi;
    game->device.bpp = (int)k->fb_state.bpp;
    game->device.pixel_format = (int)k->fb_state.pixel_format;
    game->device.is_mtk = k->fb_state.is_mtk;
    game->device.can_rotate = k->fb_state.can_rotate;
    game->device.can_hw_invert = k->fb_state.can_hw_invert;
    game->device.has_eclipse_waveform = k->fb_state.has_eclipse_wfm;
    game->device.has_color_panel = k->fb_state.has_color_panel;
    game->device.can_wait_for_submission = k->fb_state.can_wait_for_submission;
    strncpy(game->device.fbink_version, fbink_version(), sizeof(game->device.fbink_version)-1);

    refresh_fb_pointer(game);
    apply_low_latency_mode(game);

    if (game->config.keep_awake) {
        if (set_prevent_screensaver(true) == 0) k->keep_awake_set = true;
    }

    if (setup_input(game) != 0) return -1;
    if (setup_power_events(game) != 0)
        k->power_retry_due_ms = kb_now_ms() + 5000U;
    game->device.input_devices = k->input_count;
    for (int i = 0; i < k->input_count; ++i) {
        if (k->input[i].grabbed) {
            game->device.touch_grab_active = true;
            break;
        }
    }

    kb_signal_quit = 0;
    install_signal_handlers(k);
    return 0;
}

static void kindle_shutdown(KBGame *game) {
    KBKindle *k = game ? (KBKindle *)game->backend : NULL;
    if (!k) return;

    restore_signal_handlers(k);
    teardown_power_events(k);
    teardown_input(k);

    if (k->keep_awake_set)
        (void)set_prevent_screensaver(false);

    if (k->fbfd >= 0) {
        if (k->mtk_fast_mode_set) {
            (void)fbink_mtk_toggle_auto_reagl(k->fbfd, true);
            k->mtk_fast_mode_set = false;
            game->device.mtk_fast_mode_active = false;
        }
        fbink_close(k->fbfd);
        k->fbfd = -1;
    }

    if (game->config.restore_ui_on_exit)
        (void)request_native_repaint();

    if (k->lockfd >= 0) {
        flock(k->lockfd, LOCK_UN);
        close(k->lockfd);
    }
    free(k);
    game->backend = NULL;
}

const KBBackendOps kb_backend_kindle_ops = {
    kindle_init,
    kindle_shutdown,
    kindle_present,
    kindle_poll
};

#endif
