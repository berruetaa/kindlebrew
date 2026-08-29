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
#define KB_VERSION_MINOR 3
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
    KB_EVENT_TIMER,
    KB_EVENT_SUSPEND,
    KB_EVENT_RESUME,
    KB_EVENT_RESIZE,
    KB_EVENT_ORIENTATION,
    KB_EVENT_FD,
    KB_EVENT_QUIT
} KBEventType;

typedef enum {
    KB_FD_READABLE = 1u << 0,
    KB_FD_HANGUP   = 1u << 1,
    KB_FD_ERROR    = 1u << 2,
    KB_FD_INVALID  = 1u << 3
} KBFdEventFlags;

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
    int width;
    int height;
    int orientation;          /* degrees clockwise; -1 when a Kindle gyro code is ambiguous */
    int source;               /* backend-specific source value, 0 if unknown */
    uint64_t duration_ms;     /* suspend duration when known */
} KBEvent;

typedef struct {
    const char *app_id;       /* stable [A-Za-z0-9._-] id for persistent data */
    const char *title;
    int width;                  /* 0 = backend/native width */
    int height;                 /* 0 = backend/native height */
    uint8_t background;
    bool grab_touch;
    bool keep_awake;
    bool restore_ui_on_exit;
    bool low_latency_mode;      /* enables safe device-specific latency optimizations */
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
    bool direct_framebuffer_y8;
    bool mtk_fast_mode_active;
    bool touch_grab_active;
    int input_devices;
    char fbink_version[32];
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
void kb_config_defaults(KBConfig *config);
KBGame *kb_create(const KBConfig *config);
void kb_destroy(KBGame *game);
const char *kb_last_error(const KBGame *game);
const KBCanvas *kb_canvas(const KBGame *game);
uint8_t *kb_pixels(KBGame *game);
const KBDeviceInfo *kb_device_info(const KBGame *game);
const KBStats *kb_stats(const KBGame *game);
uint64_t kb_now_ms(void);
int kb_write_diagnostics(const KBGame *game, const char *path);

/* Damage & presentation */
void kb_damage(KBGame *game, KBRect rect);
void kb_damage_mono(KBGame *game, KBRect rect, bool monochrome);
int kb_present(KBGame *game, KBRefreshMode mode);
int kb_force_clean(KBGame *game);

/* Events, external file descriptors & timers */
int kb_poll_event(KBGame *game, KBEvent *event, int timeout_ms);
/* Watches are caller-owned; KBGE never closes fd. id must be unique per game. */
int kb_watch_fd(KBGame *game, int id, int fd);
int kb_unwatch_fd(KBGame *game, int id);
int kb_timer_start(KBGame *game, int id, unsigned delay_ms, unsigned repeat_ms);
int kb_timer_cancel(KBGame *game, int id);
void kb_timer_cancel_all(KBGame *game);

/* Deterministic RNG. A non-zero seed is chosen automatically at kb_create(). */
void kb_rng_seed(KBGame *game, uint64_t seed);
uint32_t kb_random_u32(KBGame *game);
uint32_t kb_random_range(KBGame *game, uint32_t upper_exclusive);

/* Persistent user data. kb_data_path creates the app directory when needed. */
int kb_data_path(KBGame *game, const char *filename, char *out, size_t out_size);
int kb_save_atomic(const char *path, const void *data, size_t size);
void *kb_load_file(const char *path, size_t *size_out);
void kb_free(void *ptr);

/* Drawing: 0 = black, 255 = white. Drawing functions mark damage automatically. */
void kb_clear(KBGame *game, uint8_t gray);
void kb_fill_rect(KBGame *game, KBRect rect, uint8_t gray);
void kb_draw_rect(KBGame *game, KBRect rect, int thickness, uint8_t gray);
void kb_draw_line(KBGame *game, int x0, int y0, int x1, int y1, int thickness, uint8_t gray);
void kb_fill_circle(KBGame *game, int cx, int cy, int radius, uint8_t gray);
void kb_blit_gray8(KBGame *game, int x, int y, const uint8_t *src, int width, int height, int src_stride);
/* Straight-alpha Gray8 compositing. alpha=0 preserves dst; alpha=255 copies src. */
void kb_blit_gray8_alpha(KBGame *game, int x, int y, const uint8_t *src, const uint8_t *alpha,
                         int width, int height, int src_stride);
void kb_invert_rect(KBGame *game, KBRect rect);

/* Built-in public-domain 8x8 ASCII bitmap font. bg_gray < 0 means transparent. */
KBRect kb_measure_text8(const char *text, int scale);
void kb_draw_text8(KBGame *game, int x, int y, const char *text, int scale,
                   uint8_t fg_gray, int bg_gray);

#ifdef __cplusplus
}
#endif
#endif
