/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../src/game2048.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void blank(G2048 *g) {
    memset(g, 0, sizeof(*g));
    g->rng = 1;
    g->last_spawn = -1;
}

static void test_merge_once(void) {
    G2048 g; blank(&g);
    g.cells[0]=2; g.cells[1]=2; g.cells[2]=2; g.cells[3]=2;
    assert(g2048_move(&g, G2048_LEFT));
    assert(g.cells[0]==4 && g.cells[1]==4);
    assert(g.score==8);
}

static void test_no_double_merge(void) {
    G2048 g; blank(&g);
    g.cells[0]=2; g.cells[1]=2; g.cells[2]=4;
    assert(g2048_move(&g, G2048_LEFT));
    assert(g.cells[0]==4 && g.cells[1]==4);
}

static void test_vertical(void) {
    G2048 g; blank(&g);
    g.cells[0]=8; g.cells[4]=8;
    assert(g2048_move(&g, G2048_UP));
    assert(g.cells[0]==16);
    assert(g.score==16);
}

static void test_undo(void) {
    G2048 g; blank(&g);
    g.cells[0]=2; g.cells[1]=2;
    uint16_t before[16]; memcpy(before,g.cells,sizeof(before));
    assert(g2048_move(&g,G2048_LEFT));
    assert(g2048_undo(&g));
    assert(memcmp(before,g.cells,sizeof(before))==0);
    assert(!g2048_undo(&g));
}

static void test_game_over(void) {
    G2048 g; blank(&g);
    const uint16_t full[16]={
        2,4,2,4, 4,2,4,2, 2,4,2,4, 4,2,4,2
    };
    memcpy(g.cells,full,sizeof(full));
    g2048_recompute_flags(&g);
    assert(g.game_over);
    g.cells[0]=4;
    g2048_recompute_flags(&g);
    assert(!g.game_over);
}

int main(void) {
    test_merge_once();
    test_no_double_merge();
    test_vertical();
    test_undo();
    test_game_over();
    puts("ink2048: all logic tests passed");
    return 0;
}
