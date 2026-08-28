# Kindlebrew Game Engine (KBGE) 0.2

A native game runtime designed specifically for jailbroken Kindle e-ink devices.

This is not an LCD game loop with a Kindle backend bolted on. KBGE treats the electrophoretic display, EPDC update queue, touch stack and Kindle lifecycle as first-class constraints.

## Design goals

- Event-driven games instead of fixed 60 FPS loops.
- 8-bit grayscale software canvas.
- Dirty-region rendering.
- Direct framebuffer fast path on safe Y8 layouts.
- FBInk conversion fallback when the framebuffer layout changes.
- Refresh policy that understands A2, DU, GL16 and GC16.
- Automatic ghosting budget and periodic clean refresh.
- MediaTek low-latency mode with automatic restoration.
- Timers integrated into the event loop.
- Atomic per-game persistence and deterministic RNG.
- Suspend/resume/resize/orientation lifecycle events.
- Touchscreen discovery through FBInk instead of hard-coded event nodes.
- Exclusive touch capture while a game is active.
- Clean coexistence with the Kindle framework: no rootfs modifications and no service killing by default.
- Host-testable rendering/policy core.
- Cross-compiled with the real kindlehf koxtoolchain.

## Minimal game

~~~c
#include "kbgame.h"

int main(void) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.app_id = "my-game";
    cfg.title = "My Game";

    KBGame *g = kb_create(&cfg);
    if (!g) return 1;

    kb_clear(g, 255);
    kb_fill_rect(g, (KBRect){40, 40, 120, 80}, 0);
    kb_present(g, KB_REFRESH_UI);

    for (;;) {
        KBEvent ev;
        if (kb_poll_event(g, &ev, -1) < 0) break;
        if (ev.type == KB_EVENT_QUIT) break;
        if (ev.type == KB_EVENT_TAP) {
            kb_fill_circle(g, ev.x, ev.y, 12, 0);
            kb_present(g, KB_REFRESH_UI);
        }
    }

    kb_destroy(g);
}
~~~

## Refresh modes

| KBGE mode | Kindle waveform | Intended use |
|---|---|---|
| KB_REFRESH_FAST_MONO | A2 | B/W motion, tracing, drag feedback |
| KB_REFRESH_UI | DU | Buttons, cursors, board-piece movement |
| KB_REFRESH_TEXT | GL16 | Text-heavy screens |
| KB_REFRESH_GRAY | GC16 | Grayscale art, antialiasing, fidelity |
| KB_REFRESH_CLEAN | flashing GC16 | Ghost cleanup / scene transitions |
| KB_REFRESH_AUTO | DU or GC16 | Safe default based on changed pixels |

A2 is stateful. KBGE conditions the first A2 request with GC16 and conditions the first frame after leaving A2. Do not bypass the policy and spam A2 directly.

## Build

Host tests:

~~~sh
make -C engine test
~~~

Kindle:

~~~sh
make -C engine kindle FBINK_DIR=vendor/FBInk
~~~

The CI workflow pins the KindleModding kindlehf toolchain and the same FBInk revision used by the modern jailbreak stack.

## Runtime behavior on Kindle

At startup KBGE takes an exclusive process lock, opens and initializes FBInk, detects framebuffer/device capabilities, allocates a Gray8 canvas, discovers input devices through fbink_input_scan, optionally grabs touch devices with EVIOCGRAB, enables safe low-latency driver policy where supported, asks powerd to inhibit the automatic screensaver, and starts from a clean full refresh.

A dedicated LIPC watcher feeds powerd lifecycle events into the same poll loop. On wake, KBGE revalidates FBInk. It restores the canonical canvas when geometry stayed stable; if geometry changed it queues RESIZE and waits for the game to relayout instead of flashing stale content.

At shutdown it releases touch grabs, restores MTK auto-REAGL and the screensaver policy, closes the framebuffer and asks X to repaint the Kindle UI.

## Reference title

`games/ink2048/` is the conformance/reference game. It deliberately exercises gestures, fast interactive updates, grayscale settling, timers, persistence, suspend/resume and resize.

See:

- `docs/sdk-guide.md`
- `docs/interactive-settle.md`
- `docs/hardware-matrix.md`
- `../games/ink2048/README.md`

## License

The engine is GPL-3.0-or-later because the Kindle backend is designed to statically link the GPL FBInk implementation. Packages using the engine must be distributed under compatible terms.
