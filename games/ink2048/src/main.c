/*
 * Ink2048 — reference game for Kindlebrew Game Engine.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "game2048.h"
#include "../../../engine/include/kbgame.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TIMER_SETTLE 1
#define SAVE_MAGIC UINT32_C(0x49323034) /* I204 */
#define SAVE_VERSION UINT32_C(1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t cells[16];
    uint16_t undo_cells[16];
    uint64_t score;
    uint64_t best;
    uint64_t undo_score;
    uint64_t rng;
    uint8_t undo_valid;
    uint8_t reserved[7];
    uint32_t checksum;
} DiskSave;

typedef struct {
    int margin;
    int board_x;
    int board_y;
    int board_size;
    int cell;
    int gap;
    KBRect header;
    KBRect board;
    KBRect status;
    KBRect undo_button;
    KBRect new_button;
    KBRect exit_button;
} Layout;

static uint32_t fnv1a(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = UINT32_C(2166136261);
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= UINT32_C(16777619);
    }
    return h;
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static Layout make_layout(const KBCanvas *c) {
    Layout l;
    memset(&l, 0, sizeof(l));

    int w = c->width;
    int h = c->height;
    l.margin = imax(12, w / 32);
    int header_h = imax(120, h / 7);
    int controls_h = imax(96, h / 10);
    int status_h = imax(40, h / 28);
    int available_h = h - header_h - controls_h - status_h - l.margin * 5;
    int available_w = w - l.margin * 2;
    l.board_size = imin(available_w, available_h);
    if (l.board_size < 320) l.board_size = imin(available_w, h - l.margin * 4);
    l.board_size -= l.board_size % 4;
    l.cell = l.board_size / 4;
    l.gap = imax(4, l.cell / 32);
    l.board_x = (w - l.board_size) / 2;
    l.board_y = header_h + l.margin * 2;

    l.header = (KBRect){0, 0, w, header_h + l.margin};
    l.board = (KBRect){l.board_x, l.board_y, l.board_size, l.board_size};

    int status_y = l.board_y + l.board_size + l.margin;
    l.status = (KBRect){l.margin, status_y, w - l.margin * 2, status_h};

    int buttons_y = status_y + status_h + l.margin;
    int buttons_w = w - l.margin * 4;
    int bw = buttons_w / 3;
    int bh = imin(controls_h, h - buttons_y - l.margin);
    if (bh < 56) bh = 56;
    l.undo_button = (KBRect){l.margin, buttons_y, bw, bh};
    l.new_button = (KBRect){l.margin * 2 + bw, buttons_y, bw, bh};
    l.exit_button = (KBRect){l.margin * 3 + bw * 2, buttons_y, bw, bh};
    return l;
}

static int text_scale_for_box(const char *text, int max_w, int max_h, int max_scale) {
    if (!text || !*text) return 1;
    int scale = max_scale;
    while (scale > 1) {
        KBRect m = kb_measure_text8(text, scale);
        if (m.w <= max_w && m.h <= max_h) break;
        --scale;
    }
    return scale;
}

static void draw_centered_text(KBGame *kb, KBRect r, const char *text, int scale,
                               uint8_t fg, int bg) {
    KBRect m = kb_measure_text8(text, scale);
    int x = r.x + (r.w - m.w) / 2;
    int y = r.y + (r.h - m.h) / 2;
    kb_draw_text8(kb, x, y, text, scale, fg, bg);
}

static uint8_t tile_gray(uint16_t value) {
    if (!value) return 248;
    int exp = 0;
    while (value > 1) {
        value = (uint16_t)(value >> 1);
        ++exp;
    }
    static const uint8_t shades[] = {
        248, 238, 226, 210, 190, 168, 144, 120,
        96, 76, 58, 42, 30, 20, 12, 4
    };
    if (exp >= (int)(sizeof(shades) / sizeof(shades[0]))) exp = (int)(sizeof(shades) / sizeof(shades[0])) - 1;
    return shades[exp];
}

static void draw_header(KBGame *kb, const G2048 *g, const Layout *l) {
    kb_fill_rect(kb, l->header, 255);

    int title_scale = text_scale_for_box("INK 2048", l->header.w / 2, l->header.h / 2, 4);
    kb_draw_text8(kb, l->margin, l->margin, "INK 2048", title_scale, 0, -1);

    char score[48], best[48];
    snprintf(score, sizeof(score), "SCORE %llu", (unsigned long long)g->score);
    snprintf(best, sizeof(best), "BEST %llu", (unsigned long long)g->best);

    int s = text_scale_for_box(score, l->header.w - l->margin * 2, 28, 2);
    int y = l->margin + title_scale * 8 + l->margin / 2;
    kb_draw_text8(kb, l->margin, y, score, s, 0, -1);
    KBRect bm = kb_measure_text8(best, s);
    kb_draw_text8(kb, l->header.w - l->margin - bm.w, y, best, s, 0, -1);

    kb_draw_line(kb, l->margin, l->header.h - 3, l->header.w - l->margin, l->header.h - 3, 3, 0);
}

static void draw_tile(KBGame *kb, const Layout *l, int index, uint16_t value, bool fast) {
    int row = index / 4;
    int col = index % 4;
    KBRect cell = {
        l->board_x + col * l->cell,
        l->board_y + row * l->cell,
        l->cell,
        l->cell
    };
    KBRect tile = {
        cell.x + l->gap,
        cell.y + l->gap,
        cell.w - l->gap * 2,
        cell.h - l->gap * 2
    };

    uint8_t bg;
    uint8_t fg;
    if (fast) {
        bool dark = value >= 128;
        bg = dark ? 0 : 255;
        fg = dark ? 255 : 0;
    } else {
        bg = tile_gray(value);
        fg = bg < 105 ? 255 : 0;
    }

    kb_fill_rect(kb, tile, bg);
    kb_draw_rect(kb, tile, imax(2, l->gap / 2), fast ? fg : 0);

    if (value) {
        char number[16];
        snprintf(number, sizeof(number), "%u", (unsigned)value);
        int scale = text_scale_for_box(number, tile.w - l->gap * 2, tile.h - l->gap * 2, 5);
        draw_centered_text(kb, tile, number, scale, fg, -1);
    }
}

static void draw_board(KBGame *kb, const G2048 *g, const Layout *l, bool fast) {
    kb_fill_rect(kb, l->board, fast ? 255 : 224);
    kb_draw_rect(kb, l->board, imax(3, l->gap), 0);
    for (int i = 0; i < 16; ++i) draw_tile(kb, l, i, g->cells[i], fast);
}

static void draw_status(KBGame *kb, const G2048 *g, const Layout *l) {
    kb_fill_rect(kb, l->status, 255);
    const char *msg = "SWIPE TO MOVE";
    if (g->game_over) msg = "NO MOVES - NEW OR UNDO";
    else if (g->won) msg = "2048! KEEP GOING";
    int scale = text_scale_for_box(msg, l->status.w, l->status.h, 2);
    draw_centered_text(kb, l->status, msg, scale, 0, -1);
}

static void draw_button(KBGame *kb, KBRect r, const char *label, bool enabled, bool primary) {
    uint8_t bg = primary ? 0 : 255;
    uint8_t fg = primary ? 255 : 0;
    if (!enabled) {
        bg = 235;
        fg = 140;
    }
    kb_fill_rect(kb, r, bg);
    kb_draw_rect(kb, r, 3, enabled ? 0 : 140);
    int scale = text_scale_for_box(label, r.w - 12, r.h - 12, 2);
    draw_centered_text(kb, r, label, scale, fg, -1);
}

static void draw_controls(KBGame *kb, const G2048 *g, const Layout *l) {
    draw_button(kb, l->undo_button, "UNDO", g->undo_valid, false);
    draw_button(kb, l->new_button, "NEW", true, true);
    draw_button(kb, l->exit_button, "EXIT", true, false);
}

static void draw_all(KBGame *kb, const G2048 *g, const Layout *l, bool fast) {
    kb_clear(kb, 255);
    draw_header(kb, g, l);
    draw_board(kb, g, l, fast);
    draw_status(kb, g, l);
    draw_controls(kb, g, l);
}

static void draw_dynamic(KBGame *kb, const G2048 *g, const Layout *l, bool fast) {
    draw_header(kb, g, l);
    draw_board(kb, g, l, fast);
    draw_status(kb, g, l);
    draw_controls(kb, g, l);
}

static bool contains(KBRect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static void fill_disk_save(DiskSave *d, const G2048 *g) {
    memset(d, 0, sizeof(*d));
    d->magic = SAVE_MAGIC;
    d->version = SAVE_VERSION;
    memcpy(d->cells, g->cells, sizeof(d->cells));
    memcpy(d->undo_cells, g->undo_cells, sizeof(d->undo_cells));
    d->score = g->score;
    d->best = g->best;
    d->undo_score = g->undo_score;
    d->rng = g->rng;
    d->undo_valid = g->undo_valid ? 1U : 0U;
    d->checksum = fnv1a(d, offsetof(DiskSave, checksum));
}

static bool restore_disk_save(G2048 *g, const DiskSave *d) {
    if (!g || !d || d->magic != SAVE_MAGIC || d->version != SAVE_VERSION) return false;
    if (d->checksum != fnv1a(d, offsetof(DiskSave, checksum))) return false;

    memset(g, 0, sizeof(*g));
    memcpy(g->cells, d->cells, sizeof(g->cells));
    memcpy(g->undo_cells, d->undo_cells, sizeof(g->undo_cells));
    g->score = d->score;
    g->best = d->best;
    g->undo_score = d->undo_score;
    g->rng = d->rng;
    g->undo_valid = d->undo_valid != 0;
    g->last_spawn = -1;
    g2048_recompute_flags(g);
    return true;
}

static bool save_game(KBGame *kb, const G2048 *g, const char *path) {
    (void)kb;
    DiskSave d;
    fill_disk_save(&d, g);
    return kb_save_atomic(path, &d, sizeof(d)) == 0;
}

static bool load_game(const char *path, G2048 *g) {
    size_t size = 0;
    DiskSave *d = (DiskSave *)kb_load_file(path, &size);
    if (!d) return false;
    bool ok = size == sizeof(*d) && restore_disk_save(g, d);
    kb_free(d);
    return ok;
}

static G2048Direction direction_from_swipe(const KBEvent *ev) {
    int ax = ev->dx < 0 ? -ev->dx : ev->dx;
    int ay = ev->dy < 0 ? -ev->dy : ev->dy;
    if (ax >= ay) return ev->dx < 0 ? G2048_LEFT : G2048_RIGHT;
    return ev->dy < 0 ? G2048_UP : G2048_DOWN;
}

static void interactive_present(KBGame *kb, const G2048 *g, const Layout *l, bool *fast_mode) {
    draw_dynamic(kb, g, l, true);
    (void)kb_present(kb, KB_REFRESH_UI);
    (void)kb_timer_start(kb, TIMER_SETTLE, 520, 0);
    *fast_mode = true;
}

int main(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.app_id = "ink2048";
    cfg.title = "Ink 2048";
    cfg.partial_refresh_limit = 18;
    cfg.clean_interval_ms = 60000;

    KBGame *kb = kb_create(&cfg);
    if (!kb) {
        fprintf(stderr, "ink2048: KBGE init failed\n");
        return 1;
    }

    char save_path[512];
    if (kb_data_path(kb, "save-v1.bin", save_path, sizeof(save_path)) != 0) {
        fprintf(stderr, "ink2048: %s\n", kb_last_error(kb));
        kb_destroy(kb);
        return 1;
    }

    G2048 game;
    if (!load_game(save_path, &game)) {
        uint64_t seed = ((uint64_t)kb_random_u32(kb) << 32) | kb_random_u32(kb);
        g2048_new(&game, seed, 0);
        (void)save_game(kb, &game, save_path);
    }

    Layout layout = make_layout(kb_canvas(kb));
    draw_all(kb, &game, &layout, false);
    (void)kb_present(kb, KB_REFRESH_CLEAN);

    bool running = true;
    bool fast_mode = false;

    while (running) {
        KBEvent ev;
        int rc = kb_poll_event(kb, &ev, -1);
        if (rc < 0) {
            fprintf(stderr, "ink2048: event error: %s\n", kb_last_error(kb));
            break;
        }
        if (rc == 0) continue;

        switch (ev.type) {
            case KB_EVENT_SWIPE:
                if (g2048_move(&game, direction_from_swipe(&ev))) {
                    (void)save_game(kb, &game, save_path);
                    interactive_present(kb, &game, &layout, &fast_mode);
                }
                break;

            case KB_EVENT_TAP:
                if (contains(layout.undo_button, ev.x, ev.y) && game.undo_valid) {
                    if (g2048_undo(&game)) {
                        (void)save_game(kb, &game, save_path);
                        interactive_present(kb, &game, &layout, &fast_mode);
                    }
                } else if (contains(layout.new_button, ev.x, ev.y)) {
                    uint64_t best = game.best;
                    uint64_t seed = ((uint64_t)kb_random_u32(kb) << 32) | kb_random_u32(kb);
                    g2048_new(&game, seed, best);
                    (void)save_game(kb, &game, save_path);
                    kb_timer_cancel(kb, TIMER_SETTLE);
                    fast_mode = false;
                    draw_all(kb, &game, &layout, false);
                    (void)kb_present(kb, KB_REFRESH_CLEAN);
                } else if (contains(layout.exit_button, ev.x, ev.y)) {
                    running = false;
                }
                break;

            case KB_EVENT_TIMER:
                if (ev.id == TIMER_SETTLE && fast_mode) {
                    draw_dynamic(kb, &game, &layout, false);
                    (void)kb_present(kb, KB_REFRESH_GRAY);
                    fast_mode = false;
                }
                break;

            case KB_EVENT_SUSPEND:
                (void)save_game(kb, &game, save_path);
                kb_timer_cancel(kb, TIMER_SETTLE);
                break;

            case KB_EVENT_RESUME:
                if (fast_mode) {
                    draw_dynamic(kb, &game, &layout, false);
                    (void)kb_present(kb, KB_REFRESH_GRAY);
                    fast_mode = false;
                }
                break;

            case KB_EVENT_RESIZE:
                layout = make_layout(kb_canvas(kb));
                kb_timer_cancel(kb, TIMER_SETTLE);
                fast_mode = false;
                draw_all(kb, &game, &layout, false);
                (void)kb_present(kb, KB_REFRESH_CLEAN);
                break;

            case KB_EVENT_KEY:
                if (ev.value == 1 && (ev.key == 1 || ev.key == 102)) running = false;
                break;

            case KB_EVENT_QUIT:
                running = false;
                break;

            default:
                break;
        }
    }

    (void)save_game(kb, &game, save_path);
    kb_destroy(kb);
    return 0;
}
