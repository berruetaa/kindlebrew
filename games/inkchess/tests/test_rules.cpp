#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "../src/game_rules.hpp"

using namespace chess;
using namespace inkchess;

static std::uint64_t perft(Board& board, int depth) {
    Movelist moves;
    movegen::legalmoves(moves, board);
    if (depth == 1) return static_cast<std::uint64_t>(moves.size());

    std::uint64_t nodes = 0;
    for (const auto& move : moves) {
        board.makeMove(move);
        nodes += perft(board, depth - 1);
        board.unmakeMove(move);
    }
    return nodes;
}

static Move legal_uci(const Board& board, const std::string& text) {
    const Move candidate = uci::uciToMove(board, text);
    Movelist moves;
    movegen::legalmoves(moves, board);
    for (const auto& move : moves) {
        if (move == candidate) return move;
    }
    return Move::NO_MOVE;
}

static void play(Board& board, const char* text) {
    const Move move = legal_uci(board, text);
    assert(move != Move::NO_MOVE);
    board.makeMove(move);
}

static void test_perft_regressions() {
    struct Case {
        const char* fen;
        int depth;
        std::uint64_t nodes;
    };

    // Deliberately use substantial but CI-friendly depths. These positions
    // exercise castling, en-passant, promotions, checks and pinned pieces.
    const Case cases[] = {
        {constants::STARTPOS, 5, 4865609ULL},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         4, 4085603ULL},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
         4, 422333ULL},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
         4, 2103487ULL},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
         4, 3894594ULL},
    };

    for (const auto& tc : cases) {
        Board board(tc.fen);
        const auto original = board.getFen();
        const auto got = perft(board, tc.depth);
        assert(got == tc.nodes);
        assert(board.getFen() == original);
    }
}

static void test_historical_castling_regressions() {
    // GNOME Chess historically regressed by allowing castling through a square
    // attacked by a pawn. Keep those exact ideas as permanent regression tests.
    {
        Board board("k7/8/8/8/8/8/6p1/4K2R w K - 0 1");
        assert(legal_uci(board, "e1g1") == Move::NO_MOVE);
    }
    {
        Board board("7k/8/8/8/8/8/4p3/R3K3 w Q - 0 1");
        assert(legal_uci(board, "e1c1") == Move::NO_MOVE);
    }

    // In-check and rook/king rights must also prevent castling.
    {
        Board board("k3r3/8/8/8/8/8/8/4K2R w K - 0 1");
        assert(legal_uci(board, "e1g1") == Move::NO_MOVE);
    }
    {
        Board board("k7/8/8/8/8/8/8/4K2R w - - 0 1");
        assert(legal_uci(board, "e1g1") == Move::NO_MOVE);
    }
}

static void test_special_moves_and_san() {
    {
        Board board("7k/8/8/pP6/8/8/8/7K w - a6 0 1");
        Move ep = legal_uci(board, "b5a6");
        assert(ep != Move::NO_MOVE);
        assert(ep.typeOf() == Move::ENPASSANT);
        assert(uci::moveToSan(board, ep) == "bxa6");
        board.makeMove(ep);
        assert(board.at(Square("a5")) == Piece::NONE);
    }

    {
        Board board("7k/P7/8/8/8/8/8/7K w - - 0 1");
        for (const char piece : {'q', 'r', 'b', 'n'}) {
            std::string move = "a7a8";
            move += piece;
            assert(legal_uci(board, move) != Move::NO_MOVE);
        }
        assert(legal_uci(board, "a7a8k") == Move::NO_MOVE);
    }

    {
        Board board("k7/8/8/8/8/8/1R6/1R5K w - - 0 1");
        Move mate = uci::parseSan(board, "Ra1#");
        assert(board.isLegal(mate));
        assert(uci::moveToSan(board, mate) == "Ra1#");
    }
}

static void test_fide_draw_policy() {
    // Threefold is claimable, not automatically terminal.
    Board repetition;
    const char* cycle[] = {
        "g1f3", "g8f6", "f3g1", "f6g8",
        "g1f3", "g8f6", "f3g1"
    };
    for (const char* move : cycle) play(repetition, move);

    Move intended = legal_uci(repetition, "f6g8");
    assert(intended != Move::NO_MOVE);
    auto preclaim = draw_claims_after_move(repetition, intended);
    assert(preclaim.threefold_repetition);
    assert(terminal_state(repetition).reason == TerminalReason::NONE);

    repetition.makeMove(intended);
    auto claim = current_draw_claims(repetition);
    assert(claim.threefold_repetition);
    assert(terminal_state(repetition).reason == TerminalReason::NONE);

    // Continue the same cycle until the initial position has occurred five times.
    for (int cycle_no = 0; cycle_no < 2; ++cycle_no) {
        play(repetition, "g1f3");
        play(repetition, "g8f6");
        play(repetition, "f3g1");
        play(repetition, "f6g8");
    }
    play(repetition, "g1f3");
    play(repetition, "g8f6");
    play(repetition, "f3g1");
    play(repetition, "f6g8");
    assert(terminal_state(repetition).reason == TerminalReason::FIVEFOLD_REPETITION);

    // The move creating the fifth occurrence is automatic, not an intended
    // threefold claim. The application must commit it before ending the game.
    {
        Board fivefold;
        for (int cycle_no = 0; cycle_no < 3; ++cycle_no) {
            play(fivefold, "g1f3");
            play(fivefold, "g8f6");
            play(fivefold, "f3g1");
            play(fivefold, "f6g8");
        }
        play(fivefold, "g1f3");
        play(fivefold, "g8f6");
        play(fivefold, "f3g1");
        const Move final = legal_uci(fivefold, "f6g8");
        assert(final != Move::NO_MOVE);
        assert(!draw_claims_after_move(fivefold, final).any());
        fivefold.makeMove(final);
        assert(terminal_state(fivefold).reason == TerminalReason::FIVEFOLD_REPETITION);
    }

    {
        Board fifty("7k/8/8/8/8/8/8/R6K w - - 100 51");
        assert(current_draw_claims(fifty).fifty_move_rule);
        assert(terminal_state(fifty).reason == TerminalReason::NONE);
    }

    {
        Board seventy_five("7k/8/8/8/8/8/8/R6K w - - 150 76");
        assert(terminal_state(seventy_five).reason == TerminalReason::SEVENTY_FIVE_MOVES);
    }

    // The 150th halfmove is automatic and must not open a 50-move claim UI.
    {
        Board before_75("7k/8/8/8/8/8/8/R6K w - - 149 75");
        const Move final = legal_uci(before_75, "a1a2");
        assert(final != Move::NO_MOVE);
        assert(!draw_claims_after_move(before_75, final).any());
        before_75.makeMove(final);
        assert(terminal_state(before_75).reason == TerminalReason::SEVENTY_FIVE_MOVES);
    }

    // Checkmate takes precedence over the 75-move automatic draw.
    {
        Board mate("7k/6Q1/5K2/8/8/8/8/8 b - - 150 76");
        auto terminal = terminal_state(mate);
        assert(terminal.reason == TerminalReason::CHECKMATE);
        assert(terminal.result == GameResult::LOSE);
    }


    // The mating 150th halfmove remains mate, not a claim or automatic draw.
    {
        Board before_mate("7k/8/5KQ1/8/8/8/8/8 w - - 149 75");
        const Move mate = legal_uci(before_mate, "g6g7");
        assert(mate != Move::NO_MOVE);
        assert(!draw_claims_after_move(before_mate, mate).any());
        before_mate.makeMove(mate);
        assert(terminal_state(before_mate).reason == TerminalReason::CHECKMATE);
    }
}

static void test_fen_make_unmake_roundtrip() {
    Board board;
    const std::string initial = board.getFen();
    Movelist legal;
    movegen::legalmoves(legal, board);

    for (const auto& move : legal) {
        const std::string san = uci::moveToSan(board, move);
        const std::string text = uci::moveToUci(move);
        board.makeMove(move);
        board.unmakeMove(move);
        assert(board.getFen() == initial);
        assert(legal_uci(board, text) == move);
        assert(uci::parseSan(board, san) == move);
    }
}

int main() {
    test_perft_regressions();
    test_historical_castling_regressions();
    test_special_moves_and_san();
    test_fide_draw_policy();
    test_fen_make_unmake_roundtrip();
    std::cout << "InkChess rules tests: OK\n";
    return 0;
}
