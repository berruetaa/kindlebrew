/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

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

static int headless_poll(KBGame *game, KBEvent *event, int timeout_ms) {
    (void)game; (void)timeout_ms;
    memset(event, 0, sizeof(*event));
    return 0;
}

const KBBackendOps kb_backend_headless_ops = {
    headless_init,
    headless_shutdown,
    headless_present,
    headless_poll
};
