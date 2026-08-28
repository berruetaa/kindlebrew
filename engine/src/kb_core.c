/*
 * Kindlebrew Game Engine core
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void kb_config_defaults(KBConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->background = 255;
    config->grab_touch = true;
    config->keep_awake = true;
    config->restore_ui_on_exit = true;
    config->auto_clean = true;
    config->partial_refresh_limit = 24;
    config->clean_interval_ms = 45000;
    config->accumulated_coverage_x100 = 250;
    config->tap_timeout_ms = 350;
    config->double_tap_ms = 450;
    config->hold_ms = 650;
}

static KBConfig kb_default_config(void) {
    KBConfig c;
    kb_config_defaults(&c);
    return c;
}

uint64_t kb_now_ms(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
    }
#endif
    return (uint64_t) time(NULL) * 1000ULL;
}

void kb_set_error(KBGame *game, const char *fmt, ...) {
    if (!game) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(game->error, sizeof(game->error), fmt, ap);
    va_end(ap);
}

const char *kb_last_error(const KBGame *game) {
    return game ? game->error : "invalid game";
}

const KBCanvas *kb_canvas(const KBGame *game) {
    return game ? &game->canvas : NULL;
}

uint8_t *kb_pixels(KBGame *game) {
    return game ? game->canvas.pixels : NULL;
}

const KBDeviceInfo *kb_device_info(const KBGame *game) {
    return game ? &game->device : NULL;
}

const KBStats *kb_stats(const KBGame *game) {
    return game ? &game->stats : NULL;
}

int kb_write_diagnostics(const KBGame *game, const char *path) {
    if (!game || !path || !*path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    const KBDeviceInfo *d = &game->device;
    const KBStats *s = &game->stats;
    fprintf(f, "kbgame=%d.%d.%d\n", KB_VERSION_MAJOR, KB_VERSION_MINOR, KB_VERSION_PATCH);
    fprintf(f, "device_name=%s\n", d->device_name);
    fprintf(f, "device_codename=%s\n", d->device_codename);
    fprintf(f, "device_platform=%s\n", d->device_platform);
    fprintf(f, "screen=%dx%d\n", d->width, d->height);
    fprintf(f, "dpi=%d\n", d->dpi);
    fprintf(f, "bpp=%d\n", d->bpp);
    fprintf(f, "pixel_format=%d\n", d->pixel_format);
    fprintf(f, "is_mtk=%d\n", d->is_mtk ? 1 : 0);
    fprintf(f, "can_rotate=%d\n", d->can_rotate ? 1 : 0);
    fprintf(f, "can_hw_invert=%d\n", d->can_hw_invert ? 1 : 0);
    fprintf(f, "has_eclipse_waveform=%d\n", d->has_eclipse_waveform ? 1 : 0);
    fprintf(f, "has_color_panel=%d\n", d->has_color_panel ? 1 : 0);
    fprintf(f, "can_wait_for_submission=%d\n", d->can_wait_for_submission ? 1 : 0);
    fprintf(f, "direct_framebuffer_y8=%d\n", d->direct_framebuffer_y8 ? 1 : 0);
    fprintf(f, "touch_grab_active=%d\n", d->touch_grab_active ? 1 : 0);
    fprintf(f, "input_devices=%d\n", d->input_devices);
    fprintf(f, "fbink_version=%s\n", d->fbink_version);
    fprintf(f, "presents=%llu\n", (unsigned long long)s->presents);
    fprintf(f, "partial_presents=%llu\n", (unsigned long long)s->partial_presents);
    fprintf(f, "clean_presents=%llu\n", (unsigned long long)s->clean_presents);
    fprintf(f, "dirty_pixels=%llu\n", (unsigned long long)s->dirty_pixels);
    fprintf(f, "refreshed_pixels=%llu\n", (unsigned long long)s->refreshed_pixels);
    fprintf(f, "last_present_ms=%llu\n", (unsigned long long)s->last_present_ms);
    fprintf(f, "last_clean_ms=%llu\n", (unsigned long long)s->last_clean_ms);
    fprintf(f, "partial_limit=%u\n", game->config.partial_refresh_limit);
    fprintf(f, "clean_interval_ms=%u\n", game->config.clean_interval_ms);
    fprintf(f, "coverage_x100=%u\n", game->config.accumulated_coverage_x100);

    return fclose(f) == 0 ? 0 : -1;
}

KBRect kb_rect_clip(KBRect r, int width, int height) {
    if (r.w <= 0 || r.h <= 0 || width <= 0 || height <= 0) return (KBRect){0,0,0,0};
    int64_t x0 = r.x;
    int64_t y0 = r.y;
    int64_t x1 = (int64_t)r.x + r.w;
    int64_t y1 = (int64_t)r.y + r.h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    if (x1 <= x0 || y1 <= y0) return (KBRect){0,0,0,0};
    return (KBRect){(int)x0,(int)y0,(int)(x1-x0),(int)(y1-y0)};
}

KBRect kb_rect_union(KBRect a, KBRect b) {
    if (kb_rect_empty(a)) return b;
    if (kb_rect_empty(b)) return a;
    int x0 = a.x < b.x ? a.x : b.x;
    int y0 = a.y < b.y ? a.y : b.y;
    int ax1 = a.x + a.w, bx1 = b.x + b.w;
    int ay1 = a.y + a.h, by1 = b.y + b.h;
    int x1 = ax1 > bx1 ? ax1 : bx1;
    int y1 = ay1 > by1 ? ay1 : by1;
    return (KBRect){x0,y0,x1-x0,y1-y0};
}

bool kb_rect_empty(KBRect r) {
    return r.w <= 0 || r.h <= 0;
}

uint64_t kb_rect_area(KBRect r) {
    if (kb_rect_empty(r)) return 0;
    return (uint64_t)(unsigned)r.w * (uint64_t)(unsigned)r.h;
}

bool kb_rect_near(KBRect a, KBRect b, int gap) {
    if (kb_rect_empty(a) || kb_rect_empty(b)) return false;
    return a.x <= b.x + b.w + gap &&
           b.x <= a.x + a.w + gap &&
           a.y <= b.y + b.h + gap &&
           b.y <= a.y + a.h + gap;
}

void kb_damage_reset(KBGame *game) {
    if (!game) return;
    memset(&game->damage, 0, sizeof(game->damage));
}

static void kb_damage_recount(KBGame *game) {
    uint64_t area = 0;
    for (int i = 0; i < game->damage.count; ++i) area += kb_rect_area(game->damage.rects[i]);
    game->damage.area = area;
}

void kb_damage_add(KBGame *game, KBRect rect, bool monochrome) {
    if (!game) return;
    rect = kb_rect_clip(rect, game->canvas.width, game->canvas.height);
    if (kb_rect_empty(rect)) return;

    if (!monochrome) game->damage.has_gray = true;

    for (int i = 0; i < game->damage.count; ++i) {
        if (!kb_rect_near(game->damage.rects[i], rect, 2)) continue;
        game->damage.rects[i] = kb_rect_union(game->damage.rects[i], rect);

        /* Merge transitively after expansion. */
        for (int j = 0; j < game->damage.count; ) {
            if (j != i && kb_rect_near(game->damage.rects[i], game->damage.rects[j], 2)) {
                game->damage.rects[i] = kb_rect_union(game->damage.rects[i], game->damage.rects[j]);
                game->damage.rects[j] = game->damage.rects[game->damage.count - 1];
                --game->damage.count;
                if (i == game->damage.count) i = j;
                continue;
            }
            ++j;
        }
        kb_damage_recount(game);
        return;
    }

    if (game->damage.count < KB_MAX_DIRTY_RECTS) {
        game->damage.rects[game->damage.count++] = rect;
        kb_damage_recount(game);
        return;
    }

    /* Too fragmented: a single larger EPDC update is cheaper than a storm of tiny ioctls. */
    KBRect all = rect;
    for (int i = 0; i < game->damage.count; ++i) all = kb_rect_union(all, game->damage.rects[i]);
    game->damage.rects[0] = all;
    game->damage.count = 1;
    game->damage.area = kb_rect_area(all);
}

KBRect kb_damage_bounds(const KBGame *game) {
    if (!game || game->damage.count == 0) return (KBRect){0,0,0,0};
    KBRect all = game->damage.rects[0];
    for (int i = 1; i < game->damage.count; ++i) all = kb_rect_union(all, game->damage.rects[i]);
    return all;
}

void kb_damage(KBGame *game, KBRect rect) {
    /* Raw writes are conservative: assume grayscale unless caller says otherwise. */
    kb_damage_add(game, rect, false);
}

void kb_damage_mono(KBGame *game, KBRect rect, bool monochrome) {
    kb_damage_add(game, rect, monochrome);
}

void kb_policy_reset(KBGame *game, uint64_t now_ms) {
    if (!game) return;
    memset(&game->refresh, 0, sizeof(game->refresh));
    game->refresh.last_clean_ms = now_ms;
}

static bool kb_policy_needs_clean(KBGame *game, uint64_t now_ms, uint64_t dirty_area) {
    if (!game->config.auto_clean) return false;

    uint64_t screen = (uint64_t)(unsigned)game->canvas.width * (uint64_t)(unsigned)game->canvas.height;
    unsigned limit = game->config.partial_refresh_limit ? game->config.partial_refresh_limit : 24;
    unsigned interval = game->config.clean_interval_ms ? game->config.clean_interval_ms : 45000;
    unsigned coverage = game->config.accumulated_coverage_x100 ? game->config.accumulated_coverage_x100 : 250;

    if (game->refresh.partial_count >= limit) return true;
    if (game->refresh.last_clean_ms && now_ms - game->refresh.last_clean_ms >= interval) return true;
    if (screen && (game->refresh.accumulated_pixels + dirty_area) * 100ULL >= screen * coverage) return true;
    return false;
}

KBRefreshMode kb_policy_choose(KBGame *game, KBRefreshMode requested, uint64_t now_ms, uint64_t dirty_area) {
    if (requested == KB_REFRESH_CLEAN) {
        game->refresh.a2_active = false;
        return KB_REFRESH_CLEAN;
    }

    if (kb_policy_needs_clean(game, now_ms, dirty_area)) {
        game->refresh.a2_active = false;
        return KB_REFRESH_CLEAN;
    }

    /*
     * A2 has a "from B&W" requirement. The first FAST_MONO frame is deliberately
     * promoted to a non-flashing high-fidelity update. Subsequent frames use A2.
     * Leaving A2 gets one GC16 frame before normal policy resumes.
     */
    if (requested == KB_REFRESH_FAST_MONO) {
        if (!game->refresh.a2_active) {
            game->refresh.a2_active = true;
            return KB_REFRESH_GRAY;
        }
        return KB_REFRESH_FAST_MONO;
    }

    if (game->refresh.a2_active) {
        game->refresh.a2_active = false;
        return KB_REFRESH_GRAY;
    }

    if (requested != KB_REFRESH_AUTO) return requested;

    /* Conservative AUTO: DU for strictly B/W damage, GC16 for real grayscale. */
    return game->damage.has_gray ? KB_REFRESH_GRAY : KB_REFRESH_UI;
}

void kb_policy_commit(KBGame *game, KBRefreshMode actual, uint64_t now_ms, uint64_t refreshed_area) {
    if (!game) return;
    if (actual == KB_REFRESH_CLEAN) {
        game->refresh.partial_count = 0;
        game->refresh.accumulated_pixels = 0;
        game->refresh.last_clean_ms = now_ms;
        game->stats.clean_presents++;
        game->stats.last_clean_ms = now_ms;
    } else {
        game->refresh.partial_count++;
        game->refresh.accumulated_pixels += refreshed_area;
        game->stats.partial_presents++;
    }
    game->stats.presents++;
    game->stats.refreshed_pixels += refreshed_area;
    game->stats.last_present_ms = now_ms;
}

int kb_event_push(KBGame *game, const KBEvent *event) {
    if (!game || !event) return -1;
    unsigned next = (game->event_tail + 1U) % KB_EVENT_QUEUE_CAP;
    if (next == game->event_head) {
        /* Drop oldest motion-like event rather than deadlocking the game loop. */
        game->event_head = (game->event_head + 1U) % KB_EVENT_QUEUE_CAP;
    }
    game->events[game->event_tail] = *event;
    game->event_tail = next;
    return 0;
}

int kb_event_pop(KBGame *game, KBEvent *event) {
    if (!game || !event || game->event_head == game->event_tail) return 0;
    *event = game->events[game->event_head];
    game->event_head = (game->event_head + 1U) % KB_EVENT_QUEUE_CAP;
    return 1;
}

KBGame *kb_create(const KBConfig *config) {
    KBGame *game = calloc(1, sizeof(*game));
    if (!game) return NULL;

    game->config = kb_default_config();
    kb_rng_seed(game, 0);
    if (config) {
        KBConfig in = *config;
        if (!in.partial_refresh_limit) in.partial_refresh_limit = game->config.partial_refresh_limit;
        if (!in.clean_interval_ms) in.clean_interval_ms = game->config.clean_interval_ms;
        if (!in.accumulated_coverage_x100) in.accumulated_coverage_x100 = game->config.accumulated_coverage_x100;
        if (!in.tap_timeout_ms) in.tap_timeout_ms = game->config.tap_timeout_ms;
        if (!in.double_tap_ms) in.double_tap_ms = game->config.double_tap_ms;
        if (!in.hold_ms) in.hold_ms = game->config.hold_ms;
        game->config = in;
    }

#ifdef KB_KINDLE
    game->ops = &kb_backend_kindle_ops;
#else
    game->ops = &kb_backend_headless_ops;
#endif

    if (game->ops->init(game) != 0) {
        if (game->ops->shutdown) game->ops->shutdown(game);
        free(game);
        return NULL;
    }

    if (game->canvas.width <= 0 || game->canvas.height <= 0) {
        kb_set_error(game, "backend returned invalid canvas dimensions");
        game->ops->shutdown(game);
        free(game);
        return NULL;
    }

    if (!game->canvas.pixels) {
        size_t n = (size_t)game->canvas.width * (size_t)game->canvas.height;
        game->canvas.pixels = malloc(n);
        if (!game->canvas.pixels) {
            kb_set_error(game, "out of memory allocating %zu-byte canvas", n);
            game->ops->shutdown(game);
            free(game);
            return NULL;
        }
        game->canvas.stride = game->canvas.width;
    }

    memset(game->canvas.pixels, game->config.background,
           (size_t)game->canvas.stride * (size_t)game->canvas.height);
    kb_damage_reset(game);
    kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height},
                  game->config.background == 0 || game->config.background == 255);
    kb_policy_reset(game, kb_now_ms());

    /* Start every real session from a known e-ink state. */
#ifdef KB_KINDLE
    if (kb_present(game, KB_REFRESH_CLEAN) != 0) {
        game->ops->shutdown(game);
        free(game->canvas.pixels);
        free(game);
        return NULL;
    }
#endif
    return game;
}

void kb_destroy(KBGame *game) {
    if (!game) return;
    if (game->ops && game->ops->shutdown) game->ops->shutdown(game);
    free(game->canvas.pixels);
    free(game);
}

int kb_present(KBGame *game, KBRefreshMode requested) {
    if (!game || !game->ops || !game->ops->present) return -1;
    if (game->damage.count == 0 && requested != KB_REFRESH_CLEAN) return 0;

    uint64_t now = kb_now_ms();
    uint64_t dirty = game->damage.area;
    KBRefreshMode actual = kb_policy_choose(game, requested, now, dirty);
    bool flashing = actual == KB_REFRESH_CLEAN;

    KBRect full = {0,0,game->canvas.width,game->canvas.height};
    const KBRect *rects = game->damage.rects;
    int count = game->damage.count;
    KBRect collapsed;

    if (flashing || count == 0) {
        rects = &full;
        count = 1;
    } else if (count > 4) {
        collapsed = kb_damage_bounds(game);
        rects = &collapsed;
        count = 1;
    }

    uint64_t refreshed = 0;
    for (int i = 0; i < count; ++i) refreshed += kb_rect_area(rects[i]);

    if (game->ops->present(game, rects, count, actual, flashing) != 0) return -1;

    game->stats.dirty_pixels += dirty;
    kb_policy_commit(game, actual, now, refreshed);
    kb_damage_reset(game);
    return 0;
}

int kb_force_clean(KBGame *game) {
    if (!game) return -1;
    kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height}, false);
    return kb_present(game, KB_REFRESH_CLEAN);
}

int kb_poll_event(KBGame *game, KBEvent *event, int timeout_ms) {
    if (!game || !event) return -1;
    if (kb_event_pop(game, event)) return 1;

    uint64_t now = kb_now_ms();
    if (kb_timer_pop_due(game, event, now)) return 1;

    if (!game->ops || !game->ops->poll_event) return 0;

    int effective_timeout = kb_timer_timeout(game, timeout_ms, now);
    int rc = game->ops->poll_event(game, event, effective_timeout);
    if (rc != 0) return rc;

    now = kb_now_ms();
    if (kb_event_pop(game, event)) return 1;
    if (kb_timer_pop_due(game, event, now)) return 1;
    return 0;
}
