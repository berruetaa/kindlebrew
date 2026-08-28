# Interactive -> settle rendering

Interactive e-ink software should optimize for perceived response, not frame rate.

Ink 2048 is the reference implementation of the KBGE interactive-settle pattern.

## Phase 1: interaction

When the player swipes, update game state immediately and render a simplified high-contrast version with a low-latency waveform.

~~~c
draw_board(game, true);
kb_present(kb, KB_REFRESH_UI);
kb_timer_start(kb, SETTLE_TIMER, 520, 0);
~~~

The image does not need to be the prettiest possible state. It needs to confirm the gesture quickly.

## Phase 2: debounce

Every new interaction replaces/restarts the settle timer. A player making several moves therefore pays for several fast updates instead of several expensive grayscale updates.

## Phase 3: settle

When the user stops interacting, render the final high-fidelity state:

~~~c
draw_board(game, false);
kb_present(kb, KB_REFRESH_GRAY);
~~~

The eye experiences a responsive game followed by a clean stabilized page.

## Why this beats a fake FPS

A 10 FPS loop on e-ink can still queue panel work faster than the pigment can settle. It burns CPU, creates collisions and ghosting, and may make input feel slower.

Interactive-settle instead couples panel cost to meaningful state transitions.

## Recommended timings

500-650 ms is a useful starting debounce window for board/menu interaction. It is not a protocol constant. Faster repeated-action games can use shorter windows and stricter monochrome rendering; turn-based games can settle immediately.

## Standard rule

If a game has a bursty interaction phase followed by visual rest, prefer interactive-settle over continuous animation.
