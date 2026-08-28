# KBGE 0.2 SDK guide

KBGE is the native e-ink game runtime used by Kindlebrew reference titles.

## The standard loop

A KBGE game is event-driven. There is no mandatory FPS and no reason to redraw because a clock tick happened.

~~~c
KBConfig cfg;
kb_config_defaults(&cfg);
cfg.app_id = "my-game";
cfg.title = "My Game";

KBGame *game = kb_create(&cfg);
if (!game) return 1;

draw_everything(game);
kb_present(game, KB_REFRESH_CLEAN);

for (;;) {
    KBEvent ev;
    int rc = kb_poll_event(game, &ev, -1);
    if (rc < 0 || ev.type == KB_EVENT_QUIT) break;

    switch (ev.type) {
        case KB_EVENT_TAP:
        case KB_EVENT_SWIPE:
            update_state(&ev);
            draw_changed_state(game);
            kb_present(game, KB_REFRESH_AUTO);
            break;
        case KB_EVENT_SUSPEND:
            save_now();
            break;
        case KB_EVENT_RESIZE:
            rebuild_layout(kb_canvas(game));
            draw_everything(game);
            kb_present(game, KB_REFRESH_CLEAN);
            break;
        default:
            break;
    }
}

kb_destroy(game);
~~~

## Canvas ownership

The Gray8 canvas is authoritative. Game code never treats /dev/fb0 as persistent state.

Built-in drawing calls mark damage automatically. If code writes through kb_pixels(), it must call kb_damage() or kb_damage_mono() for the modified region.

## Events

Input events:
- TOUCH_DOWN / TOUCH_MOVE / TOUCH_UP
- TAP / DOUBLE_TAP / HOLD / SWIPE
- KEY

Runtime events:
- TIMER
- SUSPEND
- RESUME
- RESIZE
- ORIENTATION
- QUIT

ORIENTATION carries the raw Kindle accelerometer value in event.value. event.orientation is 0/90/180/270 only when the mapping is unambiguous across known Kindle kernel families; otherwise it is -1. RESIZE is the authoritative signal that layout dimensions actually changed.

## Timers

Use kb_timer_start instead of a busy loop or scattered sleeps.

~~~c
kb_timer_start(game, 7, 500, 0);      /* one shot */
kb_timer_start(game, 8, 1000, 1000);  /* repeating */
~~~

Timers participate in the same poll deadline as input. Repeating timers advance from their prior deadline to avoid long-term drift and collapse missed periods after a stall.

## Persistence

Set a stable app_id and use kb_data_path. On Kindle the engine stores data below /mnt/us/kindlebrew-data/<app-id>/.

~~~c
char path[512];
kb_data_path(game, "save-v1.bin", path, sizeof(path));
kb_save_atomic(path, &state, sizeof(state));
~~~

kb_save_atomic writes a same-directory temporary file, fsyncs it and renames it over the destination. Version and checksum your own file format; Ink 2048 is the reference implementation.

Save after meaningful state transitions and on KB_EVENT_SUSPEND. Do not wait until process exit.

## Randomness

KBGE provides a tiny deterministic RNG:

~~~c
uint32_t card = kb_random_range(game, 52);
~~~

Use kb_rng_seed with a fixed seed for deterministic tests/replays. A runtime seed is generated automatically when the engine is created.

## Lifecycle

KBGE listens to com.lab126.powerd through lipc-wait-event. Games do not spawn LIPC watchers themselves.

On wake, if framebuffer geometry is unchanged, KBGE restores the canonical canvas. If geometry changed, KBGE queues RESIZE and deliberately waits for the game to relayout before presenting.

## Low-latency mode

low_latency_mode is enabled by default. On MediaTek Kindles the backend disables automatic REAGL promotion while the game is active, matching the strategy used by modern KOReader Kindle backends. The setting is restored on exit.

Games should still request semantic refresh modes instead of knowing about MTK:

~~~c
kb_present(game, KB_REFRESH_UI);
~~~

## Threading

KBGE 0.2 assumes rendering and event dispatch live on one main thread. Worker threads may compute game AI or decode data, but framebuffer/input calls should remain on the engine thread.

## Error policy

Check kb_create and negative returns from poll/present. Diagnostic strings come from kb_last_error(). Hardware probes should also call kb_write_diagnostics().

## Library identity across upgrades

Keep `library.json.document_name` stable. If a rename is unavoidable, add the previous managed Scriptlet names to `legacy_document_names`; the generated helper cleans the old document and its `.sdr` metadata before installing the new one.
