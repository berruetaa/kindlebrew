/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KB_INTERNAL_H
#define KB_INTERNAL_H

#include "../include/kbgame.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KB_MAX_DIRTY_RECTS 16
#define KB_EVENT_QUEUE_CAP 128
#define KB_MAX_TIMERS 32

typedef struct {
    KBRect rects[KB_MAX_DIRTY_RECTS];
    int count;
    uint64_t area;
    bool has_gray;
} KBDamage;

typedef struct {
    unsigned partial_count;
    uint64_t accumulated_pixels;
    uint64_t last_clean_ms;
    bool a2_active;
} KBRefreshPolicy;

typedef struct {
    int id;
    uint64_t due_ms;
    uint64_t repeat_ms;
    bool active;
} KBTimer;

typedef struct {
    int (*init)(KBGame *game);
    void (*shutdown)(KBGame *game);
    int (*present)(KBGame *game, const KBRect *rects, int count, KBRefreshMode mode, bool flashing);
    int (*poll_event)(KBGame *game, KBEvent *event, int timeout_ms);
} KBBackendOps;

struct KBGame {
    KBConfig config;
    KBCanvas canvas;
    KBDeviceInfo device;
    KBStats stats;
    KBDamage damage;
    KBRefreshPolicy refresh;
    KBEvent events[KB_EVENT_QUEUE_CAP];
    unsigned event_head;
    unsigned event_tail;
    KBTimer timers[KB_MAX_TIMERS];
    uint64_t rng_state;
    char error[256];
    void *backend;
    const KBBackendOps *ops;
};

KBRect kb_rect_clip(KBRect r, int width, int height);
KBRect kb_rect_union(KBRect a, KBRect b);
bool kb_rect_empty(KBRect r);
uint64_t kb_rect_area(KBRect r);
bool kb_rect_near(KBRect a, KBRect b, int gap);
void kb_damage_reset(KBGame *game);
void kb_damage_add(KBGame *game, KBRect rect, bool monochrome);
int kb_damage_compact(const KBRect *src, int count, KBRect *dst, int max_rects);
KBRect kb_damage_bounds(const KBGame *game);

KBRefreshMode kb_policy_choose(KBGame *game, KBRefreshMode requested, uint64_t now_ms, uint64_t dirty_area);
void kb_policy_commit(KBGame *game, KBRefreshMode actual, uint64_t now_ms, uint64_t refreshed_area);
void kb_policy_reset(KBGame *game, uint64_t now_ms);

int kb_event_push(KBGame *game, const KBEvent *event);
int kb_event_pop(KBGame *game, KBEvent *event);
int kb_timer_pop_due(KBGame *game, KBEvent *event, uint64_t now_ms);
int kb_timer_timeout(const KBGame *game, int requested_timeout_ms, uint64_t now_ms);

extern const KBBackendOps kb_backend_headless_ops;
#ifdef KB_KINDLE
extern const KBBackendOps kb_backend_kindle_ops;
#endif

void kb_set_error(KBGame *game, const char *fmt, ...);

#endif
