/*
 * Kindlebrew Game Engine canvas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#include <stdlib.h>
#include <string.h>

static bool mono(uint8_t g) { return g == 0 || g == 255; }

void kb_clear(KBGame *game, uint8_t gray) {
    if (!game || !game->canvas.pixels) return;
    for (int y = 0; y < game->canvas.height; ++y) {
        memset(game->canvas.pixels + (size_t)y * game->canvas.stride, gray, (size_t)game->canvas.width);
    }
    kb_damage_add(game, (KBRect){0,0,game->canvas.width,game->canvas.height}, mono(gray));
}

void kb_fill_rect(KBGame *game, KBRect rect, uint8_t gray) {
    if (!game || !game->canvas.pixels) return;
    rect = kb_rect_clip(rect, game->canvas.width, game->canvas.height);
    if (kb_rect_empty(rect)) return;
    for (int y = rect.y; y < rect.y + rect.h; ++y) {
        memset(game->canvas.pixels + (size_t)y * game->canvas.stride + rect.x, gray, (size_t)rect.w);
    }
    kb_damage_add(game, rect, mono(gray));
}

void kb_draw_rect(KBGame *game, KBRect rect, int thickness, uint8_t gray) {
    if (!game || thickness <= 0 || kb_rect_empty(rect)) return;
    if (thickness * 2 >= rect.w || thickness * 2 >= rect.h) {
        kb_fill_rect(game, rect, gray);
        return;
    }
    kb_fill_rect(game, (KBRect){rect.x, rect.y, rect.w, thickness}, gray);
    kb_fill_rect(game, (KBRect){rect.x, rect.y + rect.h - thickness, rect.w, thickness}, gray);
    kb_fill_rect(game, (KBRect){rect.x, rect.y + thickness, thickness, rect.h - thickness * 2}, gray);
    kb_fill_rect(game, (KBRect){rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - thickness * 2}, gray);
}

static void plot_thick(KBGame *game, int x, int y, int thickness, uint8_t gray) {
    int r = thickness > 1 ? thickness / 2 : 0;
    kb_fill_rect(game, (KBRect){x-r,y-r,thickness > 0 ? thickness : 1,thickness > 0 ? thickness : 1}, gray);
}

void kb_draw_line(KBGame *game, int x0, int y0, int x1, int y1, int thickness, uint8_t gray) {
    if (!game) return;
    if (thickness < 1) thickness = 1;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        plot_thick(game, x0, y0, thickness, gray);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void kb_fill_circle(KBGame *game, int cx, int cy, int radius, uint8_t gray) {
    if (!game || radius < 0) return;
    int x = radius;
    int y = 0;
    int err = 1 - x;
    while (x >= y) {
        kb_fill_rect(game, (KBRect){cx-x, cy+y, x*2+1, 1}, gray);
        kb_fill_rect(game, (KBRect){cx-x, cy-y, x*2+1, 1}, gray);
        kb_fill_rect(game, (KBRect){cx-y, cy+x, y*2+1, 1}, gray);
        kb_fill_rect(game, (KBRect){cx-y, cy-x, y*2+1, 1}, gray);
        ++y;
        if (err < 0) {
            err += 2*y + 1;
        } else {
            --x;
            err += 2*(y-x) + 1;
        }
    }
}

void kb_blit_gray8(KBGame *game, int x, int y, const uint8_t *src, int width, int height, int src_stride) {
    if (!game || !src || width <= 0 || height <= 0 || src_stride < width) return;
    KBRect dst = kb_rect_clip((KBRect){x,y,width,height}, game->canvas.width, game->canvas.height);
    if (kb_rect_empty(dst)) return;

    int sx = dst.x - x;
    int sy = dst.y - y;
    bool is_mono = true;
    for (int row = 0; row < dst.h; ++row) {
        const uint8_t *s = src + (size_t)(sy + row) * src_stride + sx;
        uint8_t *d = game->canvas.pixels + (size_t)(dst.y + row) * game->canvas.stride + dst.x;
        memcpy(d, s, (size_t)dst.w);
        if (is_mono) {
            for (int col = 0; col < dst.w; ++col) {
                if (!mono(s[col])) { is_mono = false; break; }
            }
        }
    }
    kb_damage_add(game, dst, is_mono);
}

void kb_invert_rect(KBGame *game, KBRect rect) {
    if (!game || !game->canvas.pixels) return;
    rect = kb_rect_clip(rect, game->canvas.width, game->canvas.height);
    if (kb_rect_empty(rect)) return;
    bool is_mono = true;
    for (int y = rect.y; y < rect.y + rect.h; ++y) {
        uint8_t *p = game->canvas.pixels + (size_t)y * game->canvas.stride + rect.x;
        for (int x = 0; x < rect.w; ++x) {
            p[x] = (uint8_t)(255U - p[x]);
            if (!mono(p[x])) is_mono = false;
        }
    }
    kb_damage_add(game, rect, is_mono);
}
