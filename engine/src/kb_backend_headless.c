/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#include <errno.h>
#include <poll.h>
#include <string.h>

static int headless_init(KBGame *game) {
    int w = game->config.width > 0 ? game->config.width : 600;
    int h = game->config.height > 0 ? game->config.height : 800;
    game->canvas.width = w;
    game->canvas.height = h;
    game->canvas.stride = w;

    memset(&game->device, 0, sizeof(game->device));
    strncpy(game->device.device_name, "headless", sizeof(game->device.device_name)-1);
    game->device.width = w;
    game->device.height = h;
    game->device.dpi = 167;
    game->device.bpp = 8;
    return 0;
}

static void headless_shutdown(KBGame *game) {
    (void)game;
}

static int headless_present(KBGame *game, const KBRect *rects, int count, KBRefreshMode mode, bool flashing) {
    (void)game; (void)rects; (void)count; (void)mode; (void)flashing;
    return 0;
}

static unsigned fd_flags(short revents) {
    unsigned flags = 0;
    if (revents & (POLLIN | POLLPRI)) flags |= KB_FD_READABLE;
    if (revents & POLLHUP) flags |= KB_FD_HANGUP;
    if (revents & POLLERR) flags |= KB_FD_ERROR;
    if (revents & POLLNVAL) flags |= KB_FD_INVALID;
    return flags;
}

static int headless_poll(KBGame *game, KBEvent *event, int timeout_ms) {
    struct pollfd pfds[KB_MAX_FD_WATCHES];
    int watch_index[KB_MAX_FD_WATCHES];
    int n = 0;

    for (int i = 0; i < KB_MAX_FD_WATCHES; ++i) {
        if (!game->fd_watches[i].active) continue;
        pfds[n].fd = game->fd_watches[i].fd;
        pfds[n].events = POLLIN | POLLPRI;
        pfds[n].revents = 0;
        watch_index[n] = i;
        ++n;
    }

    int rc = poll(pfds, (nfds_t)n, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) return 0;
        kb_set_error(game, "poll(external fd) failed");
        return -1;
    }
    if (rc == 0) {
        memset(event, 0, sizeof(*event));
        return 0;
    }

    for (int p = 0; p < n; ++p) {
        unsigned flags = fd_flags(pfds[p].revents);
        if (!flags) continue;
        KBFdWatch *watch = &game->fd_watches[watch_index[p]];
        memset(event, 0, sizeof(*event));
        event->type = KB_EVENT_FD;
        event->time_ms = kb_now_ms();
        event->id = watch->id;
        event->value = (int)flags;
        return 1;
    }

    memset(event, 0, sizeof(*event));
    return 0;
}

const KBBackendOps kb_backend_headless_ops = {
    headless_init,
    headless_shutdown,
    headless_present,
    headless_poll
};
