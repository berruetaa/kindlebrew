/*
 * Ink2048 game logic
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef INK2048_GAME_H
#define INK2048_GAME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    G2048_LEFT = 0,
    G2048_RIGHT,
    G2048_UP,
    G2048_DOWN
} G2048Direction;

typedef struct {
    uint16_t cells[16];
    uint16_t undo_cells[16];
    uint64_t score;
    uint64_t best;
    uint64_t undo_score;
    uint64_t rng;
    bool undo_valid;
    bool won;
    bool game_over;
    int last_spawn;
} G2048;

void g2048_new(G2048 *g, uint64_t seed, uint64_t best);
bool g2048_move(G2048 *g, G2048Direction dir);
bool g2048_undo(G2048 *g);
bool g2048_can_move(const G2048 *g);
void g2048_recompute_flags(G2048 *g);

#endif
