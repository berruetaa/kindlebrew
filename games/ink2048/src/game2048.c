/*
 * Ink2048 game logic
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "game2048.h"

#include <string.h>

static uint32_t rnd(G2048 *g) {
    uint64_t x = g->rng ? g->rng : UINT64_C(0x9E3779B97F4A7C15);
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g->rng = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32);
}

static int index_for(G2048Direction dir, int line, int pos) {
    switch (dir) {
        case G2048_LEFT:  return line * 4 + pos;
        case G2048_RIGHT: return line * 4 + (3 - pos);
        case G2048_UP:    return pos * 4 + line;
        case G2048_DOWN:  return (3 - pos) * 4 + line;
        default:          return 0;
    }
}

static void spawn(G2048 *g) {
    int empty[16];
    int count = 0;
    for (int i = 0; i < 16; ++i) {
        if (g->cells[i] == 0) empty[count++] = i;
    }
    if (!count) {
        g->last_spawn = -1;
        return;
    }

    int slot = empty[rnd(g) % (uint32_t)count];
    g->cells[slot] = (rnd(g) % 10U == 0U) ? 4U : 2U;
    g->last_spawn = slot;
}

static bool line_move(const uint32_t in[4], uint32_t out[4], uint64_t *score_delta) {
    uint32_t compact[4] = {0,0,0,0};
    int n = 0;
    for (int i = 0; i < 4; ++i) if (in[i]) compact[n++] = in[i];

    int o = 0;
    for (int i = 0; i < n; ++i) {
        if (i + 1 < n && compact[i] == compact[i + 1]) {
            uint32_t merged = compact[i] * 2U;
            out[o++] = merged;
            *score_delta += merged;
            ++i;
        } else {
            out[o++] = compact[i];
        }
    }
    while (o < 4) out[o++] = 0;

    return memcmp(in, out, sizeof(uint32_t) * 4U) != 0;
}

bool g2048_can_move(const G2048 *g) {
    if (!g) return false;
    for (int i = 0; i < 16; ++i) if (g->cells[i] == 0) return true;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int i = r * 4 + c;
            if (c < 3 && g->cells[i] == g->cells[i + 1]) return true;
            if (r < 3 && g->cells[i] == g->cells[i + 4]) return true;
        }
    }
    return false;
}

void g2048_recompute_flags(G2048 *g) {
    if (!g) return;
    g->won = false;
    for (int i = 0; i < 16; ++i) {
        if (g->cells[i] >= 2048U) {
            g->won = true;
            break;
        }
    }
    g->game_over = !g2048_can_move(g);
}

void g2048_new(G2048 *g, uint64_t seed, uint64_t best) {
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->best = best;
    g->rng = seed ? seed : UINT64_C(0xD1B54A32D192ED03);
    g->last_spawn = -1;
    spawn(g);
    spawn(g);
    g->changed_mask = UINT16_C(0xFFFF);
    g2048_recompute_flags(g);
}

bool g2048_move(G2048 *g, G2048Direction dir) {
    if (!g) return false;

    uint32_t before[16];
    memcpy(before, g->cells, sizeof(before));
    uint64_t old_score = g->score;
    uint64_t old_rng = g->rng;
    uint64_t score_delta = 0;
    g->changed_mask = 0;
    bool changed = false;

    for (int line = 0; line < 4; ++line) {
        uint32_t in[4], out[4] = {0,0,0,0};
        for (int p = 0; p < 4; ++p) in[p] = g->cells[index_for(dir, line, p)];
        if (line_move(in, out, &score_delta)) changed = true;
        for (int p = 0; p < 4; ++p) g->cells[index_for(dir, line, p)] = out[p];
    }

    if (!changed) {
        memcpy(g->cells, before, sizeof(before));
        return false;
    }

    memcpy(g->undo_cells, before, sizeof(before));
    g->undo_score = old_score;
    g->undo_rng = old_rng;
    g->undo_valid = true;
    g->score += score_delta;
    if (g->score > g->best) g->best = g->score;
    spawn(g);
    for (int i = 0; i < 16; ++i) {
        if (before[i] != g->cells[i]) g->changed_mask |= (uint16_t)(1U << i);
    }
    g2048_recompute_flags(g);
    return true;
}

bool g2048_undo(G2048 *g) {
    if (!g || !g->undo_valid) return false;

    uint32_t before[16];
    memcpy(before, g->cells, sizeof(before));

    memcpy(g->cells, g->undo_cells, sizeof(g->cells));
    g->score = g->undo_score;
    g->rng = g->undo_rng;
    g->undo_valid = false;
    g->last_spawn = -1;
    g->changed_mask = 0;
    for (int i = 0; i < 16; ++i) {
        if (before[i] != g->cells[i]) g->changed_mask |= (uint16_t)(1U << i);
    }
    g2048_recompute_flags(g);
    return true;
}
