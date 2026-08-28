/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../src/kb_internal.h"

#include <assert.h>
#include <stdio.h>

static void test_rects(void) {
    KBRect r = kb_rect_clip((KBRect){-10,-20,50,60}, 100, 100);
    assert(r.x == 0 && r.y == 0 && r.w == 40 && r.h == 40);

    KBRect u = kb_rect_union((KBRect){10,10,10,10}, (KBRect){15,5,20,10});
    assert(u.x == 10 && u.y == 5 && u.w == 25 && u.h == 15);

    assert(kb_rect_near((KBRect){0,0,10,10}, (KBRect){12,0,10,10}, 2));
    assert(!kb_rect_near((KBRect){0,0,10,10}, (KBRect){13,0,10,10}, 2));
}

static void test_damage(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 100;
    cfg.height = 100;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    kb_damage_mono(g, (KBRect){1,1,10,10}, true);
    kb_damage_mono(g, (KBRect){12,1,10,10}, true);
    assert(g->damage.count == 1);
    KBRect b = kb_damage_bounds(g);
    assert(b.x == 1 && b.y == 1 && b.w == 21 && b.h == 10);
    assert(!g->damage.has_gray);

    kb_damage(g, (KBRect){50,50,5,5});
    assert(g->damage.has_gray);
    kb_destroy(g);
}

static void test_canvas(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 32;
    cfg.height = 24;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    kb_clear(g, 255);
    kb_fill_rect(g, (KBRect){5,5,10,8}, 0);
    const KBCanvas *c = kb_canvas(g);
    assert(c->pixels[5 * c->stride + 5] == 0);
    assert(c->pixels[0] == 255);

    kb_invert_rect(g, (KBRect){5,5,1,1});
    assert(c->pixels[5 * c->stride + 5] == 255);

    kb_destroy(g);
}

static void test_text(void) {
    KBRect m = kb_measure_text8("AB\nC", 2);
    assert(m.w == 32 && m.h == 32);

    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 64;
    cfg.height = 32;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    kb_clear(g, 255);
    kb_damage_reset(g);
    kb_draw_text8(g, 0, 0, "A", 1, 0, -1);
    assert(g->damage.count == 1);
    assert(g->damage.rects[0].w == 8 && g->damage.rects[0].h == 8);

    const KBCanvas *c = kb_canvas(g);
    int black = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (c->pixels[y*c->stride+x] == 0) ++black;
    assert(black > 0);

    kb_destroy(g);
}

static void test_refresh_policy(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 100;
    cfg.height = 100;
    cfg.auto_clean = true;
    cfg.partial_refresh_limit = 2;
    cfg.clean_interval_ms = 999999;
    cfg.accumulated_coverage_x100 = 10000;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    kb_policy_reset(g, 1000);

    /* First A2 request is deliberately conditioned with GC16. */
    KBRefreshMode m = kb_policy_choose(g, KB_REFRESH_FAST_MONO, 1100, 100);
    assert(m == KB_REFRESH_GRAY);
    kb_policy_commit(g, m, 1100, 100);

    m = kb_policy_choose(g, KB_REFRESH_FAST_MONO, 1200, 100);
    assert(m == KB_REFRESH_FAST_MONO);
    kb_policy_commit(g, m, 1200, 100);

    /* Partial limit upgrades the next frame to a clean flash. */
    m = kb_policy_choose(g, KB_REFRESH_UI, 1300, 100);
    assert(m == KB_REFRESH_CLEAN);
    kb_policy_commit(g, m, 1300, 100);
    assert(g->refresh.partial_count == 0);

    /* Leaving A2 without a forced clean gets a GC16 transition. */
    g->config.partial_refresh_limit = 100;
    kb_policy_reset(g, 2000);
    m = kb_policy_choose(g, KB_REFRESH_FAST_MONO, 2100, 100);
    assert(m == KB_REFRESH_GRAY);
    kb_policy_commit(g, m, 2100, 100);
    m = kb_policy_choose(g, KB_REFRESH_FAST_MONO, 2200, 100);
    assert(m == KB_REFRESH_FAST_MONO);
    kb_policy_commit(g, m, 2200, 100);
    m = kb_policy_choose(g, KB_REFRESH_UI, 2300, 100);
    assert(m == KB_REFRESH_GRAY);

    kb_destroy(g);
}

int main(void) {
    test_rects();
    test_damage();
    test_canvas();
    test_text();
    test_refresh_policy();
    puts("kbgame: all host tests passed");
    return 0;
}
