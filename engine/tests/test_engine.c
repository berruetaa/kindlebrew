/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../src/kb_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static void test_damage_accumulator_pressure(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 1000;
    cfg.height = 1000;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    for (int i = 0; i < KB_MAX_DIRTY_RECTS + 1; ++i) {
        int x = (i % 5) * 180;
        int y = (i / 5) * 180;
        kb_damage_mono(g, (KBRect){x, y, 10, 10}, true);
    }

    assert(g->damage.count == KB_MAX_DIRTY_RECTS);
    KBRect bounds = kb_damage_bounds(g);
    uint64_t bounds_area = kb_rect_area(bounds);
    assert(g->damage.area < bounds_area);
    assert(g->damage.count > 1);

    kb_destroy(g);
}

static void test_damage_compaction(void) {
    KBRect src[6] = {
        {0,0,10,10},
        {12,0,10,10},
        {100,0,10,10},
        {112,0,10,10},
        {0,100,10,10},
        {100,100,10,10}
    };
    KBRect out[KB_MAX_DIRTY_RECTS];

    int n = kb_damage_compact(src, 6, out, 4);
    assert(n == 4);

    /*
     * The two obvious adjacent pairs should be cheaper than bridging the
     * hundred-pixel gaps. We do not rely on output order; verify that no
     * compacted rectangle spans almost the whole synthetic screen.
     */
    for (int i = 0; i < n; ++i) {
        assert(out[i].w < 80);
        assert(out[i].h < 80);
    }

    n = kb_damage_compact(src, 6, out, 1);
    assert(n == 1);
    assert(out[0].x == 0 && out[0].y == 0);
    assert(out[0].w == 122 && out[0].h == 110);
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

static void test_event_queue_pressure(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 16;
    cfg.height = 16;
    KBGame *g = kb_create(&cfg);
    assert(g);

    /* Consecutive motion for the same finger is coalesced to the newest point. */
    KBEvent move;
    memset(&move, 0, sizeof(move));
    move.type = KB_EVENT_TOUCH_MOVE;
    move.id = 3;
    for (int i = 0; i < 1000; ++i) {
        move.x = i;
        assert(kb_event_push(g, &move) == 0);
    }
    KBEvent out;
    assert(kb_event_pop(g, &out) == 1);
    assert(out.type == KB_EVENT_TOUCH_MOVE && out.x == 999);
    assert(kb_event_pop(g, &out) == 0);

    /* Fill with meaningful non-critical events, then prove lifecycle wins. */
    KBEvent tap;
    memset(&tap, 0, sizeof(tap));
    tap.type = KB_EVENT_TAP;
    for (int i = 0; i < KB_EVENT_QUEUE_CAP - 1; ++i) {
        tap.id = i;
        assert(kb_event_push(g, &tap) == 0);
    }

    KBEvent extra = tap;
    extra.id = 9999;
    assert(kb_event_push(g, &extra) == 1); /* rejected rather than evicting */

    KBEvent suspend_ev;
    memset(&suspend_ev, 0, sizeof(suspend_ev));
    suspend_ev.type = KB_EVENT_SUSPEND;
    assert(kb_event_push(g, &suspend_ev) == 0);

    bool saw_suspend = false;
    while (kb_event_pop(g, &out)) {
        if (out.type == KB_EVENT_SUSPEND) saw_suspend = true;
    }
    assert(saw_suspend);

    /*
     * Regression: if the oldest event is critical but later entries are taps,
     * a new critical event must evict a tap, not the older lifecycle event.
     */
    KBEvent drain;
    while (kb_event_pop(g, &drain)) {}
    memset(&suspend_ev, 0, sizeof(suspend_ev));
    suspend_ev.type = KB_EVENT_SUSPEND;
    assert(kb_event_push(g, &suspend_ev) == 0);
    for (int i = 1; i < KB_EVENT_QUEUE_CAP - 1; ++i) {
        tap.id = i;
        assert(kb_event_push(g, &tap) == 0);
    }
    KBEvent quit_ev;
    memset(&quit_ev, 0, sizeof(quit_ev));
    quit_ev.type = KB_EVENT_QUIT;
    assert(kb_event_push(g, &quit_ev) == 0);

    bool kept_suspend = false;
    bool saw_quit = false;
    while (kb_event_pop(g, &out)) {
        if (out.type == KB_EVENT_SUSPEND) kept_suspend = true;
        if (out.type == KB_EVENT_QUIT) saw_quit = true;
    }
    assert(kept_suspend && saw_quit);
    kb_destroy(g);
}

static void test_long_suspend_timer_catchup(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 8;
    cfg.height = 8;
    KBGame *g = kb_create(&cfg);
    assert(g);

    assert(kb_timer_start(g, 77, 1, 1) == 0);

    KBTimer *timer = NULL;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (g->timers[i].active && g->timers[i].id == 77) {
            timer = &g->timers[i];
            break;
        }
    }
    assert(timer);

    timer->due_ms = 1000;
    KBEvent ev;
    uint64_t simulated_wake = UINT64_C(86400000) + 1000; /* 24 hours late */
    assert(kb_timer_pop_due(g, &ev, simulated_wake) == 1);
    assert(ev.type == KB_EVENT_TIMER && ev.id == 77);
    assert(timer->due_ms > simulated_wake);
    assert(timer->due_ms == simulated_wake + 1);

    kb_destroy(g);
}

static void test_timer_deadline_saturation(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 8;
    cfg.height = 8;
    KBGame *g = kb_create(&cfg);
    assert(g);

    assert(kb_timer_start(g, 88, 1, 1) == 0);
    KBTimer *timer = NULL;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (g->timers[i].active && g->timers[i].id == 88) {
            timer = &g->timers[i];
            break;
        }
    }
    assert(timer);
    timer->due_ms = 0;
    timer->repeat_ms = 1;

    KBEvent ev;
    assert(kb_timer_pop_due(g, &ev, UINT64_MAX) == 1);
    assert(ev.type == KB_EVENT_TIMER && ev.id == 88);
    assert(!timer->active);
    assert(kb_timer_pop_due(g, &ev, UINT64_MAX) == 0);

    kb_destroy(g);
}

static void test_runtime_services(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.app_id = "kbgame-tests";
    cfg.width = 32;
    cfg.height = 32;
    KBGame *a = kb_create(&cfg);
    KBGame *b = kb_create(&cfg);
    assert(a && b);

    kb_rng_seed(a, 0x12345678ULL);
    kb_rng_seed(b, 0x12345678ULL);
    for (int i = 0; i < 16; ++i) assert(kb_random_u32(a) == kb_random_u32(b));
    for (int i = 0; i < 128; ++i) assert(kb_random_range(a, 7) < 7);

    assert(kb_timer_start(a, 42, 0, 0) == 0);
    KBEvent ev;
    assert(kb_poll_event(a, &ev, 0) == 1);
    assert(ev.type == KB_EVENT_TIMER && ev.id == 42);

    char path[512];
    assert(kb_data_path(a, "state.bin", path, sizeof(path)) == 0);
    const char payload[] = "atomic-save";
    assert(kb_save_atomic(path, payload, sizeof(payload)) == 0);
    size_t size = 0;
    char *loaded = kb_load_file(path, &size);
    assert(loaded && size == sizeof(payload));
    assert(memcmp(loaded, payload, size) == 0);
    kb_free(loaded);
    unlink(path);

    kb_destroy(a);
    kb_destroy(b);
}

static int fail_present(KBGame *game, const KBRect *rects, int count,
                        KBRefreshMode mode, bool flashing) {
    (void)game;
    (void)rects;
    (void)count;
    (void)mode;
    (void)flashing;
    return -1;
}

static const KBBackendOps failing_present_ops = {
    .init = NULL,
    .shutdown = NULL,
    .present = fail_present,
    .poll_event = NULL,
};

static void test_present_failure_rolls_back_waveform_state(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.width = 32;
    cfg.height = 32;
    KBGame *g = kb_create(&cfg);
    assert(g);

    kb_damage_reset(g);
    kb_damage_mono(g, (KBRect){0,0,8,8}, true);
    g->refresh.a2_active = false;
    g->ops = &failing_present_ops;

    assert(kb_present(g, KB_REFRESH_FAST_MONO) == -1);
    assert(!g->refresh.a2_active);
    assert(g->damage.count > 0); /* failed present must remain retryable */

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
    test_damage_accumulator_pressure();
    test_damage_compaction();
    test_canvas();
    test_text();
    test_event_queue_pressure();
    test_long_suspend_timer_catchup();
    test_timer_deadline_saturation();
    test_runtime_services();
    test_present_failure_rolls_back_waveform_state();
    test_refresh_policy();
    puts("kbgame: all host tests passed");
    return 0;
}
