# InkChess

Native e-ink chess for Kindlebrew, built on KBGE.

The rules layer is deliberately independent of rendering and Stockfish. It uses
Disservin/chess-library 0.9.4 pinned to commit
`53e6a841dcda7059a2af363d85f785ef1817304a`.

## Correctness policy

InkChess does not use `Board::isGameOver()` directly because tournament draw
semantics matter:

- threefold repetition and 50 moves are claimable;
- fivefold repetition and 75 moves are automatic draws;
- checkmate takes precedence over the 75-move rule.

The app implements those FIDE semantics in `src/game_rules.hpp` and keeps
regression tests for them.

## Host rules tests

```sh
make -C games/inkchess test
```

The suite includes standard perft positions, historical GNOME Chess castling
regressions, en-passant, every promotion type, SAN/UCI round trips, FEN
make/unmake invariants and FIDE draw-claim/automatic-draw behavior.
