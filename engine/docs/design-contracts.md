# Engine contracts

These are constraints, not suggestions. Games using KBGE should be designed around them.

## 1. No fixed LCD frame loop

Do not redraw continuously because time passed. Redraw because state changed.

A Kindle game should normally block in kb_poll_event and present only damaged regions after an event, timer, AI result or state transition.

## 2. Refresh mode expresses intent

The game says whether a change is fast monochrome motion, UI, text, grayscale or a clean scene transition. The backend owns the device-specific waveform details.

Do not call framebuffer ioctls from game code.

## 3. A2 is a session

A2 is not a universal fast mode. Entry/exit conditioning and later ghost cleanup are part of its cost.

## 4. The canvas is canonical state

Game graphics live in the software Gray8 canvas. The physical framebuffer is a presentation target, never the authoritative game image.

This makes wake/repaint, diagnostics, format changes and headless testing tractable.

## 5. Every custom pixel write declares damage

Built-in drawing primitives do this automatically. Code that writes through kb_pixels must call kb_damage or kb_damage_mono.

Failure to mark damage is a game bug, not a backend refresh bug.

## 6. Touch ownership is temporary

KBGE may grab the touchscreen, but only for the game lifetime. It never grabs the power button and must always release grabbed descriptors.

## 7. Kindle services stay alive by default

Framework/power/frontlight services are not game dependencies to kill. If a future title proves that it needs exclusive framework suspension, that behavior must be opt-in, reversible and device-tested.

## 8. No rootfs mutation

Compatibility belongs in the package. Private libraries, shims and assets live under /mnt/us.

## 9. ABI is a build property

All release binaries are built with the kindlehf toolchain. A successful generic ARM build is not evidence of Kindle compatibility.

## 10. Hardware claims require InkLab

Host tests validate algorithms. CI validates ABI. Neither validates electrophoretic behavior.

Changes to coordinate mapping, waveform policy, MTK tuning, framebuffer fast paths or lifecycle behavior require a real-device InkLab pass before being treated as proven.
