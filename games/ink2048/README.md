# Ink 2048

Ink 2048 is both a playable Kindle game and the reference/conformance title for Kindlebrew Game Engine.

It intentionally exercises:
- swipe gesture recognition;
- fast interactive DU updates;
- debounced GC16 settling;
- Gray8 tiles and bitmap text;
- dirty-region rendering;
- timers;
- atomic save/load;
- undo;
- suspend/resume save behavior;
- resize/relayout;
- KPM packaging and Scriptlet launch.

## Controls

- Swipe: move tiles
- Undo: restore the previous successful move
- New: start a new board while preserving best score
- Exit: save and leave cleanly

## Save

The save is stored in:

~~~text
/mnt/us/kindlebrew-data/ink2048/save-v1.bin
~~~

The file has a magic/version header and FNV-1a checksum and is written atomically. Uninstalling the package intentionally preserves it.

## Rendering model

Moves first render a monochrome/high-contrast board through KB_REFRESH_UI. A 520 ms one-shot timer is then armed. If no new move arrives, the board is redrawn with its full grayscale palette through KB_REFRESH_GRAY.

See engine/docs/interactive-settle.md.

## Build

Host logic tests:

~~~sh
make -C games/ink2048 test
~~~

Kindle target after the KBGE/FBInk cross-build:

~~~sh
make -C games/ink2048 kindle ENGINE_DIR=../../engine FBINK_DIR=../../engine/vendor/FBInk
~~~
