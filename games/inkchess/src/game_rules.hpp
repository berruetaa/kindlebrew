#pragma once

#include "../third_party/chess.hpp"

namespace inkchess {

enum class TerminalReason {
    NONE = 0,
    CHECKMATE,
    STALEMATE,
    INSUFFICIENT_MATERIAL,
    FIVEFOLD_REPETITION,
    SEVENTY_FIVE_MOVES
};

struct TerminalState {
    TerminalReason reason = TerminalReason::NONE;
    chess::GameResult result = chess::GameResult::NONE;
};

struct DrawClaims {
    bool threefold_repetition = false;
    bool fifty_move_rule = false;

    [[nodiscard]] bool any() const noexcept {
        return threefold_repetition || fifty_move_rule;
    }
};

[[nodiscard]] inline TerminalState terminal_state(const chess::Board& board) {
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);

    // FIDE 9.6.2 explicitly gives checkmate precedence over the automatic
    // 75-move draw, so mate/stalemate must be evaluated before draw counters.
    if (legal.empty()) {
        if (board.inCheck()) {
            return {TerminalReason::CHECKMATE, chess::GameResult::LOSE};
        }
        return {TerminalReason::STALEMATE, chess::GameResult::DRAW};
    }

    if (board.isInsufficientMaterial()) {
        return {TerminalReason::INSUFFICIENT_MATERIAL, chess::GameResult::DRAW};
    }

    // chess-library's isRepetition(count) counts previous matching positions.
    // Four previous matches plus the current one is FIDE's automatic fivefold.
    if (board.isRepetition(4)) {
        return {TerminalReason::FIVEFOLD_REPETITION, chess::GameResult::DRAW};
    }

    // 75 moves by each player without pawn movement or capture = 150 halfmoves.
    if (board.halfMoveClock() >= 150) {
        return {TerminalReason::SEVENTY_FIVE_MOVES, chess::GameResult::DRAW};
    }

    return {};
}

[[nodiscard]] inline DrawClaims current_draw_claims(const chess::Board& board) {
    DrawClaims claims;
    // Two prior matching positions plus the current one is a third occurrence.
    claims.threefold_repetition = board.isRepetition(2);
    claims.fifty_move_rule = board.halfMoveClock() >= 100;
    return claims;
}

// FIDE 9.2.1 / 9.3.1 allow a player to claim before making the move that
// creates the third occurrence or completes 50 moves. The caller must present
// that choice before committing the move.
[[nodiscard]] inline DrawClaims draw_claims_after_move(const chess::Board& board,
                                                       const chess::Move& move) {
    if (!board.isLegal(move)) {
        return {};
    }
    chess::Board after = board;
    after.makeMove(move);
    return current_draw_claims(after);
}

}  // namespace inkchess
