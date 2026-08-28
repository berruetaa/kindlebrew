/*
 * InkLab: tiny on-device smoke test for Kindlebrew Game Engine.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../include/kbgame.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static void draw_home(KBGame *g) {
    const KBCanvas *c = kb_canvas(g);
    int w = c->width, h = c->height;

    kb_clear(g, 255);
    kb_draw_rect(g, (KBRect){8,8,w-16,h-16}, 4, 0);

    /* Three large e-ink-safe interaction zones. */
    int gap = w / 30;
    int bw = (w - gap * 4) / 3;
    int by = h - h / 5;
    int bh = h / 8;
    kb_fill_rect(g, (KBRect){gap,by,bw,bh}, 0);
    kb_fill_rect(g, (KBRect){gap*2+bw,by,bw,bh}, 128);
    kb_draw_rect(g, (KBRect){gap*3+bw*2,by,bw,bh}, 5, 0);

    /* Crosshair makes rotation/scaling mistakes obvious. */
    kb_draw_line(g, w/2, 20, w/2, h/3, 2, 0);
    kb_draw_line(g, w/2-w/10, h/6, w/2+w/10, h/6, 2, 0);
}

int main(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.title = "Kindlebrew InkLab";
    cfg.partial_refresh_limit = 20;

    KBGame *g = kb_create(&cfg);
    if (!g) {
        fprintf(stderr, "InkLab: failed to initialize kbgame\n");
        return 1;
    }

    const KBDeviceInfo *d = kb_device_info(g);
    fprintf(stderr, "InkLab: %s (%s) %dx%d %ddpi bpp=%d mtk=%d\n",
            d->device_name, d->device_codename, d->width, d->height,
            d->dpi, d->bpp, d->is_mtk ? 1 : 0);

    draw_home(g);
    if (kb_present(g, KB_REFRESH_CLEAN) != 0) {
        fprintf(stderr, "InkLab: present failed: %s\n", kb_last_error(g));
        kb_destroy(g);
        return 1;
    }

    bool running = true;
    bool dragging = false;
    int last_x = 0, last_y = 0;

    while (running) {
        KBEvent ev;
        int rc = kb_poll_event(g, &ev, -1);
        if (rc < 0) {
            fprintf(stderr, "InkLab: input failed: %s\n", kb_last_error(g));
            break;
        }
        if (rc == 0) continue;

        switch (ev.type) {
            case KB_EVENT_TOUCH_DOWN:
                dragging = true;
                last_x = ev.x; last_y = ev.y;
                kb_fill_circle(g, ev.x, ev.y, 9, 0);
                kb_present(g, KB_REFRESH_FAST_MONO);
                break;
            case KB_EVENT_TOUCH_MOVE:
                if (dragging) {
                    kb_draw_line(g, last_x, last_y, ev.x, ev.y, 5, 0);
                    last_x = ev.x; last_y = ev.y;
                    kb_present(g, KB_REFRESH_FAST_MONO);
                }
                break;
            case KB_EVENT_TOUCH_UP:
                dragging = false;
                break;
            case KB_EVENT_TAP:
                kb_fill_circle(g, ev.x, ev.y, 22, 96);
                kb_present(g, KB_REFRESH_GRAY);
                break;
            case KB_EVENT_DOUBLE_TAP:
                draw_home(g);
                kb_present(g, KB_REFRESH_CLEAN);
                break;
            case KB_EVENT_HOLD:
                kb_invert_rect(g, (KBRect){ev.x-45,ev.y-45,90,90});
                kb_present(g, KB_REFRESH_UI);
                break;
            case KB_EVENT_SWIPE:
                draw_home(g);
                kb_present(g, KB_REFRESH_CLEAN);
                break;
            case KB_EVENT_KEY:
                /* HOME(102), ESC(1) or frame/page keys may be used as an exit hatch. */
                if (ev.value == 1 && (ev.key == 102 || ev.key == 1)) running = false;
                break;
            case KB_EVENT_QUIT:
                running = false;
                break;
            default:
                break;
        }
    }

    kb_destroy(g);
    return 0;
}
