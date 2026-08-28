/*
 * Bitmap text renderer.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Glyph data: font8x8 by Daniel Hepper / IBM VGA fonts, Public Domain.
 */
#include "kb_internal.h"
#include "../third_party/font8x8_basic.h"

#include <string.h>

KBRect kb_measure_text8(const char *text, int scale) {
    if (!text || scale < 1) return (KBRect){0,0,0,0};
    int cols = 0, max_cols = 0, lines = 1;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '\n') {
            if (cols > max_cols) max_cols = cols;
            cols = 0;
            ++lines;
        } else {
            ++cols;
        }
    }
    if (cols > max_cols) max_cols = cols;
    return (KBRect){0,0,max_cols * 8 * scale,lines * 8 * scale};
}

static void put_scaled_pixel(KBGame *game, int x, int y, int scale, uint8_t gray) {
    for (int yy = 0; yy < scale; ++yy) {
        int py = y + yy;
        if (py < 0 || py >= game->canvas.height) continue;
        size_t off = (size_t)py * (size_t)game->canvas.stride;
        uint8_t *row = game->canvas.pixels + off;
        for (int xx = 0; xx < scale; ++xx) {
            int px = x + xx;
            if (px >= 0 && px < game->canvas.width) row[px] = gray;
        }
    }
}

void kb_draw_text8(KBGame *game, int x, int y, const char *text, int scale,
                   uint8_t fg_gray, int bg_gray) {
    if (!game || !game->canvas.pixels || !text || scale < 1) return;

    KBRect measured = kb_measure_text8(text, scale);
    if (bg_gray >= 0 && measured.w > 0 && measured.h > 0) {
        KBRect bg = kb_rect_clip((KBRect){x,y,measured.w,measured.h},
                                 game->canvas.width, game->canvas.height);
        if (!kb_rect_empty(bg)) {
            for (int row = bg.y; row < bg.y + bg.h; ++row) {
                size_t off = (size_t)row * (size_t)game->canvas.stride + (size_t)bg.x;
                memset(game->canvas.pixels + off, (uint8_t)bg_gray, (size_t)bg.w);
            }
        }
    }

    int ox = x;
    int cx = x, cy = y;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '\n') {
            cx = ox;
            cy += 8 * scale;
            continue;
        }

        unsigned char ch = *p < 128 ? *p : (unsigned char)'?';
        const unsigned char *glyph = (const unsigned char *)font8x8_basic[ch];
        for (int gy = 0; gy < 8; ++gy) {
            unsigned char bits = glyph[gy];
            for (int gx = 0; gx < 8; ++gx) {
                if ((bits >> gx) & 1U) {
                    put_scaled_pixel(game, cx + gx * scale, cy + gy * scale, scale, fg_gray);
                }
            }
        }
        cx += 8 * scale;
    }

    bool monochrome = (fg_gray == 0 || fg_gray == 255) &&
                      (bg_gray < 0 || bg_gray == 0 || bg_gray == 255);
    kb_damage_add(game, (KBRect){x,y,measured.w,measured.h}, monochrome);
}
