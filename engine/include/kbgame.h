/*
 * Kindlebrew Game Engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KBGAME_H
#define KBGAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KB_VERSION_MAJOR 0
#define KB_VERSION_MINOR 1
#define KB_VERSION_PATCH 0

typedef struct KBGame KBGame;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} KBRect;

typedef struct {
    int width;
    int height;
    int stride;
    uint8_t *pixels;
} KBCanvas;

typedef enum {
    KB_REFRESH_AUTO = 0,
    KB_REFRESH_FAST_MONO,
    KB_REFRESH_UI,
    KB_REFRESH_TEXT,
    KB_REFRESH_GRAY,
    KB_REFRESH_CLEAN
} KBRefreshMode;

typedef enum {
    KB_EVENT_NONE = 0,
    KB_EVENT_TOUCH_DOWN,
    KB_EVENT_TOUCH_MOVE,
    KB_EVENT_TOUCH_UP,
    KB_EVENT_TAP,
    KB_EVENT_DOUBLE_TAP,
    KB_EVENT_HOLD,
    KB_EVENT_SWIPE,
    KB_EVENT_KEY,
    KB_EVENT_QUIT
} KBEventType;

typedef struct {
    KBEventType type;
    uint64_t time_ms;
    int id;
    int x;
    int y;
    int start_x;
    int start_y;
    int dx;
    int dy;
    int key;
    int value;
} KBEvent;

typedef struct {
    const char *title;
    int width;                  /* 0 = backend/native width */
    int height;                 /* 0 = backend/native height */
    uint8_t background;
    bool grab_touch;
    bool keep_awake;
    bool restore_ui_on_exit;
    bool auto_clean;
    unsigned partial_refresh_limit;
    unsigned clean_interval_ms;
    unsigned accumulated_coverage_x100;
    unsigned tap_slop_px;       /* 0 = DPI-derived */
    unsigned tap_timeout_ms;    /* 0 = 350 */
    unsigned double_tap_ms;     /* 0 = 450 */
    unsigned hold_ms;           /* 0 = 650 */
    unsigned swipe_min_px;      /* 0 = DPI-derived */
} KBConfig;

typedef struct {
    char device_name[32];
    char device_codename[32];
    char device_platform[32];
    int width;
    int height;
    int dpi;
    int bpp;
    int pixel_format;
    bool is_mtk;
    bool can_rotate;
    bool can_hw_invert;
    bool has_eclipse_waveform;
    bool has_color_panel;
    bool can_wait_for_submission;
} KBDeviceInfo;

typedef struct {
    uint64_t presents;
    uint64_t partial_presents;
    uint64_t clean_presents;
    uint64_t dirty_pixels;
    uint64_t refreshed_pixels;
    uint64_t last_present_ms;
    uint64_t last_clean_ms;
} KBStats;

/* Runtime */
KBGame *kb_create(const KBConfig *config);
void kb_destroy(KBGame *game);
const char *kb_last_error(const KBGame *game);
const KBCanvas *kb_canvas(const KBGame *game);
uint8_t *kb_pixels(KBGame *game);
const KBDeviceInfo *kb_device_info(const KBGame *game);
const KBStats *kb_stats(const KBGame *game);
uint64_t kb_now_ms(void);

/* Damage & presentation */
void kb_damage(KBGame *game, KBRect rect);
void kb_damage_mono(KBGame *game, KBRect rect, bool monochrome);
int kb_present(KBGame *game, KBRefreshMode mode);
int kb_force_clean(KBGame *game);

/* Input */
int kb_poll_event(KBGame *game, KBEvent *event, int timeout_ms);

/* Drawing: 0 = black, 255 = white. Drawing functions mark damage automatically. */
void kb_clear(KBGame *game, uint8_t gray);
void kb_fill_rect(KBGame *game, KBRect rect, uint8_t gray);
void kb_draw_rect(KBGame *game, KBRect rect, int thickness, uint8_t gray);
void kb_draw_line(KBGame *game, int x0, int y0, int x1, int y1, int thickness, uint8_t gray);
void kb_fill_circle(KBGame *game, int cx, int cy, int radius, uint8_t gray);
void kb_blit_gray8(KBGame *game, int x, int y, const uint8_t *src, int width, int height, int src_stride);
void kb_invert_rect(KBGame *game, KBRect rect);

#ifdef __cplusplus
}
#endif
#endif
