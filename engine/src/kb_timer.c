/*
 * Kindlebrew Game Engine timers.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#include <limits.h>
#include <string.h>

int kb_timer_start(KBGame *game, int id, unsigned delay_ms, unsigned repeat_ms) {
    if (!game) return -1;

    KBTimer *slot = NULL;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (game->timers[i].active && game->timers[i].id == id) {
            slot = &game->timers[i];
            break;
        }
        if (!slot && !game->timers[i].active) slot = &game->timers[i];
    }
    if (!slot) {
        kb_set_error(game, "timer table is full");
        return -1;
    }

    slot->id = id;
    slot->due_ms = kb_now_ms() + (uint64_t)delay_ms;
    slot->repeat_ms = (uint64_t)repeat_ms;
    slot->active = true;
    return 0;
}

int kb_timer_cancel(KBGame *game, int id) {
    if (!game) return -1;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (game->timers[i].active && game->timers[i].id == id) {
            memset(&game->timers[i], 0, sizeof(game->timers[i]));
            return 1;
        }
    }
    return 0;
}

void kb_timer_cancel_all(KBGame *game) {
    if (!game) return;
    memset(game->timers, 0, sizeof(game->timers));
}

int kb_timer_pop_due(KBGame *game, KBEvent *event, uint64_t now_ms) {
    if (!game || !event) return 0;

    int best = -1;
    uint64_t best_due = UINT64_MAX;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (!game->timers[i].active || game->timers[i].due_ms > now_ms) continue;
        if (game->timers[i].due_ms < best_due) {
            best_due = game->timers[i].due_ms;
            best = i;
        }
    }
    if (best < 0) return 0;

    KBTimer *t = &game->timers[best];
    memset(event, 0, sizeof(*event));
    event->type = KB_EVENT_TIMER;
    event->time_ms = now_ms;
    event->id = t->id;

    if (t->repeat_ms) {
        /*
         * Advance from the old deadline, not from now, to avoid long-term drift.
         * Collapse any number of missed periods in O(1): a Kindle may have
         * slept for hours, and a millisecond timer must not loop millions of
         * times merely to catch its deadline up.
         */
        uint64_t periods = (now_ms - t->due_ms) / t->repeat_ms;
        if (periods == UINT64_MAX) {
            /*
             * There is no representable deadline strictly after now. Leaving
             * due_ms at UINT64_MAX while now is UINT64_MAX would make this
             * timer fire forever, so retire it after delivering this event.
             */
            memset(t, 0, sizeof(*t));
        } else {
            uint64_t missed = periods + 1U;
            if (missed > (UINT64_MAX - t->due_ms) / t->repeat_ms) {
                memset(t, 0, sizeof(*t));
            } else {
                t->due_ms += missed * t->repeat_ms;
            }
        }
    } else {
        memset(t, 0, sizeof(*t));
    }
    return 1;
}

int kb_timer_timeout(const KBGame *game, int requested_timeout_ms, uint64_t now_ms) {
    if (!game) return requested_timeout_ms;

    uint64_t nearest = UINT64_MAX;
    for (int i = 0; i < KB_MAX_TIMERS; ++i) {
        if (game->timers[i].active && game->timers[i].due_ms < nearest)
            nearest = game->timers[i].due_ms;
    }
    if (nearest == UINT64_MAX) return requested_timeout_ms;

    int timer_timeout;
    if (nearest <= now_ms) {
        timer_timeout = 0;
    } else {
        uint64_t delta = nearest - now_ms;
        timer_timeout = delta > (uint64_t)INT_MAX ? INT_MAX : (int)delta;
    }

    if (requested_timeout_ms < 0 || timer_timeout < requested_timeout_ms)
        return timer_timeout;
    return requested_timeout_ms;
}
