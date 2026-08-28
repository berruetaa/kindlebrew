# E-ink rendering and refresh policy

## The framebuffer is not the display

Writing bytes to /dev/fb0 changes memory. It does not move pigment. A separate EPDC update request tells the panel what rectangular region to physically refresh and with which waveform.

KBGE therefore separates software rendering into a Gray8 canvas, copying dirty pixels into the framebuffer, and scheduling an e-ink refresh.

## Waveforms

The timings below come from FBInk documentation and are approximate. Hardware, region size and collisions change real latency.

### A2
- Around 120 ms in the FBInk reference notes.
- B/W to B/W only; FBInk may force monochrome.
- Never flashes.
- Fast enough for short motion bursts but accumulates ghosting.
- KBGE conditions entry and exit instead of switching into A2 cold.

### DU
- Around 260 ms.
- Fast direct update for B/W-oriented content.
- Never flashes.
- Good for buttons, cursors, pieces and other UI movement.

### GL16
- Around 450 ms.
- Optimized for text/light content from white.
- Better fidelity than DU/A2.

### GC16
- Around 450 ms.
- High fidelity grayscale with the lowest ghosting risk of the common modes.
- A flashing GC16 is KBGE's clean/reset update.

## Automatic ghosting budget

KBGE tracks partial-present count, cumulative refreshed pixel area and elapsed time since the last clean refresh.

Current defaults trigger a clean update after any of:
- 24 partial presents;
- 45 seconds;
- cumulative partial coverage equivalent to 2.5 full screens.

Games can tune the thresholds. Disabling cleanup entirely should be exceptional.

## Dirty rectangles

Drawing operations record changed regions automatically. Overlapping or almost-touching rectangles are merged. Excessive fragmentation collapses into a larger bounding region because many tiny EPDC ioctls are generally worse than one slightly larger update.

The panel changes only when kb_present is called.

## Direct Y8 fast path

When the framebuffer is Y8, 8bpp, non-inverted, has no viewport offsets and exposes a safe FBInk mmap, KBGE copies dirty rows directly to the mapped framebuffer.

Every other layout uses fbink_print_raw_data with refresh disabled. FBInk then owns conversion and device quirks. KBGE still performs the explicit rectangular refresh afterward.

## Update fences

Kindle EPDC normally queues and merges updates. Waiting after every partial update destroys responsiveness.

KBGE therefore does not block after ordinary partial refreshes. Before a disruptive flashing clean refresh it may wait for the previous marker when FBInk reports that waits are reliable. Treat these waits like e-ink fences, not a frame limiter.

## MediaTek Kindles

FBInk exposes MTK-specific controls including swipe animation, automatic REAGL behavior, global waits and pen mode. KBGE intentionally leaves stock auto-REAGL behavior enabled by default.

FBInk documents an important constraint: disabling automatic REAGL (the so-called fast mode) changes update collection semantics and makes wait_for_any_complete unsuitable. Low-latency MTK tuning belongs in an explicit profile, never an unconditional startup tweak.
