# Native Chess port design

Status: implementation contract for `feat/kbge-chess`.

## Goal

Replace the legacy GTK/OpenGL GNOME Chess wrapper with a Kindle-native chess
application built on KBGE. The first hardware build must be a complete playable
game, not a rendering proof of concept.

The implementation deliberately separates four independently testable layers:

1. chess rules and notation;
2. Stockfish process/UCI protocol;
3. Kindle input/rendering through KBGE;
4. KPM packaging and lifecycle.

No layer may silently compensate for a failure in another layer.

## Target and hardware assumptions

The primary acceptance target is the 2024 Kindle Basic (KT6/Rossini/Bellatrix,
MediaTek MT8110, 300 dpi), while retaining the generic `kindlehf` ABI used by
KBGE.

KBGE owns framebuffer/input/power behavior. Game code must never use
`/dev/fb0`, mxcfb ioctls, hard-coded `/dev/input/event*` paths, or Kindle
service manipulation.

### E-ink policy

Chess is event-driven. There is no animation loop.

- Initial board, resize, orientation change and explicit scene reset:
  `KB_REFRESH_CLEAN`.
- Selection, legal-target hints, piece moves and buttons: small dirty regions
  with `KB_REFRESH_UI`.
- After interactive activity settles: redraw affected squares in final
  grayscale and use `KB_REFRESH_GRAY`.
- Never use A2 for normal chess play.
- Never issue a full-screen refresh for an ordinary move.
- A move redraws source/destination plus exceptional squares (capture,
  en-passant, castling, promotion) and the minimum changed HUD region.
- KBGE remains responsible for ghosting budget and MediaTek auto-REAGL policy.

## Rules authority

Do not port the 2011 GNOME Games 2.91.93 move generator.

The native game vendors the single-header `Disservin/chess-library` pinned to
commit:

`53e6a841dcda7059a2af363d85f785ef1817304a`

The library is MIT licensed and provides legal move generation, FEN, SAN,
make/unmake, repetition, fifty-move handling, insufficient-material detection
and game-over classification.

The vendored header is immutable except for a clearly documented upstream
update. Upstream provenance and license must ship with source and package
metadata.

### Rule test gates

Before a Kindle binary is produced:

- run canonical standard-chess perft positions;
- include the historical GNOME Chess regression cases, especially pawn attacks
  through castling paths, en-passant, promotion and material draws;
- round-trip FEN after make/unmake;
- round-trip SAN for every legal move in curated positions;
- verify illegal taps never mutate board state;
- verify undo restores exact FEN and repetition state.

A rules change that alters a perft count is a release blocker.

## Engine opponent

Stockfish is a separate child process. Pin the stable Stockfish 18 release and
build it for generic ARMv7 with the same `kindlehf` toolchain. Do not require
NEON on the first release.

The Stockfish executable and NNUE data required by the chosen build are bundled
inside the KPM package. Installation and first launch must not require network
access.

Runtime defaults:

- Threads = 1
- Hash = 16 MiB
- Ponder = false
- Syzygy disabled/not bundled
- no opening-book dependency

Difficulty uses `UCI_LimitStrength=true` plus `UCI_Elo` where supported.
Response latency uses a bounded `go movetime` budget rather than pretending
the Kindle is playing a tournament clock unless the user explicitly enables
timed chess.

## UCI state machine

The GUI must implement a strict line-oriented state machine, not a collection
of sleeps.

Startup:

1. fork/exec Stockfish with stdin/stdout pipes;
2. send `uci`;
3. wait for `uciok`;
4. send all options;
5. send `isready`;
6. wait for `readyok`;
7. only then enable computer play.

New game:

1. if searching, send `stop` and consume the corresponding `bestmove`;
2. send `ucinewgame`;
3. send `isready`;
4. wait for `readyok`;
5. send position and search request only after synchronization.

Search:

- send one complete `position startpos moves ...` line;
- send exactly one `go ...`;
- accept only the `bestmove` associated with the active generation;
- validate the returned move against the rules library before applying it;
- malformed/illegal `bestmove`, EOF, child exit or pipe error is an engine
  fault, never a legal game move.

Shutdown:

1. send `stop` when searching;
2. send `quit`;
3. close pipes;
4. reap child with a bounded graceful phase and terminate it if necessary;
5. never leave an orphaned Stockfish process.

Suspend cancels active search and saves game state. Resume must not apply stale
engine output from before suspend.

## KBGE external-FD contract

Stockfish stdout must participate in the same blocking poll loop as Kindle
input, timers and lifecycle events. KBGE will expose a small external-FD watch
API and a `KB_EVENT_FD` event.

Requirements:

- watched descriptors are caller-owned;
- KBGE never closes a watched descriptor;
- registration rejects invalid/duplicate ids;
- removal is idempotent;
- HUP/ERR/NVAL are surfaced to the caller;
- host backend tests exercise readable, HUP and removal cases;
- Kindle backend uses the same effective timer/hold/lifecycle deadline as
  before watches existed.

No periodic 20/50/100 ms Stockfish polling timer.

## Rendering

The board layout derives from the runtime canvas dimensions. Never hard-code a
Kindle resolution.

Pieces use GNOME Chess's default `simple` SVG set, vendored with its original W3C Software Notice and License. The SVGs are rasterized at build time into compact Gray8 + alpha masks sized for the KBGE board cells. Runtime has no SVG, Cairo, GTK, PNG or OpenGL dependency. Artwork geometry is preserved; only grayscale/raster sampling is adapted for e-ink.

KBGE gains a clipped grayscale-mask blit primitive. It must:

- support arbitrary destination clipping;
- preserve pixels outside the mask;
- mark only the clipped destination as damaged;
- have host tests for negative origins, edge clipping and mask extremes.

Touch mapping is computed from the exact board rectangle. A move is two taps:
select a friendly piece, then tap a legal target. Tapping another friendly
piece changes selection. Tapping outside the board cancels selection only when
appropriate and must never generate a move.

Promotion is an explicit four-choice overlay; queen-by-default is not used for
a human tap.

## Persistence

Persist after every completed move and on suspend.

The save format is versioned and checksummed. It stores enough data to restore
exactly:

- current FEN;
- complete UCI move history from the initial position;
- human side;
- difficulty;
- last move;
- game result;
- UI orientation/preferences needed for consistent restore.

Write atomically through `kb_save_atomic`. Corrupt or unsupported saves are
quarantined/ignored rather than partially loaded.

## Crash/failure behavior

A chess game must remain usable if Stockfish cannot start. The UI presents a
clear engine-unavailable state and permits local two-player play/new game/exit.

A rendering/present failure is fatal and exits through `kb_destroy` so touch
grabs, MediaTek policy and Kindle UI restoration are released.

## Packaging

The final `chess` KPM replaces the legacy GNOME Games wrapper but preserves
package id `chess`.

The package contains the native game, Stockfish, required license/provenance
files and library Scriptlet integration. It performs no rootfs mutation and
does not download payloads during installation.

Upgrade from legacy Chess is atomic and must not overwrite an unmanaged
extension directory.

## Hardware acceptance

CI can prove algorithms and ABI, not electrophoretic behavior. Before merging
to main, run on real KT6 hardware:

1. fresh install and launch from library and `;kpm launch chess`;
2. play white and black against Stockfish through checkmate;
3. castle both sides, en-passant and promote to all four pieces;
4. undo/new-game during human turn and engine turn;
5. suspend while idle, selected and while Stockfish is thinking;
6. resume and verify no stale move appears;
7. rotate if the device reports an orientation change;
8. play at least one long game sufficient to exercise auto-clean;
9. exit from every major UI state and verify Amazon UI touch works immediately;
10. intentionally kill Stockfish and verify graceful degradation/restart;
11. reinstall/upgrade/uninstall and verify library metadata and saved game
    handling.

Only hardware behavior observed in this matrix may be called validated.
