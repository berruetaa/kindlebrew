#include <cassert>
#include <string>

#include "../src/chess_session.hpp"

using namespace inkchess;

static int sq(char file, char rank) {
    return (rank - '1') * 8 + (file - 'a');
}

int main() {
    ChessSession game;
    game.reset(PlayMode::HUMAN_WHITE, 1600);

    auto a = game.tap_square(sq('e', '2'));
    assert(a.changed && !a.moved);
    assert(game.selected_square() == sq('e', '2'));
    assert(game.legal_target_mask() & (1ULL << sq('e', '4')));

    // Illegal target clears selection without mutating the board.
    const std::string start = game.board().getFen();
    auto bad = game.tap_square(sq('e', '5'));
    assert(bad.changed && !bad.moved);
    assert(game.board().getFen() == start);

    game.tap_square(sq('e', '2'));
    auto good = game.tap_square(sq('e', '4'));
    assert(good.moved);
    assert(game.uci_history().size() == 1 && game.uci_history()[0] == "e2e4");
    assert(game.engine_turn());

    std::uint64_t engine_dirty = 0;
    assert(game.apply_engine_move("e7e5", &engine_dirty));
    assert(engine_dirty != 0);
    assert(game.human_turn());

    // Against engine, undo from the human turn restores both plies.
    assert(game.can_undo());
    assert(game.undo() != 0);
    assert(game.board().getFen() == chess::constants::STARTPOS);
    assert(game.uci_history().empty());

    // An engine move that is syntactically valid but illegal must never enter.
    game.reset(PlayMode::HUMAN_BLACK, 1600);
    assert(game.engine_turn());
    assert(!game.apply_engine_move("e2e5"));
    assert(game.uci_history().empty());
    assert(game.apply_engine_move("e2e4"));
    assert(game.human_turn());

    // Save/restore keeps exact board and history semantics.
    const SaveData save = game.save_data();
    ChessSession restored;
    assert(restored.restore(save));
    assert(restored.board().getFen() == game.board().getFen());
    assert(restored.uci_history() == game.uci_history());

    // Local two-player permits one-ply undo.
    restored.reset(PlayMode::LOCAL_TWO_PLAYER, 1600);
    restored.tap_square(sq('g', '1'));
    auto knight = restored.tap_square(sq('f', '3'));
    assert(knight.moved);
    assert(restored.can_undo());
    restored.undo();
    assert(restored.board().getFen() == chess::constants::STARTPOS);

    // Promotion requires an explicit piece choice. Build it through restore so
    // the normal session API remains the only source of board mutation.
    SaveData promotion;
    promotion.mode = PlayMode::LOCAL_TWO_PLAYER;
    promotion.elo = 1600;
    promotion.outcome = OutcomeOverride::NONE;
    promotion.moves = {
        "a2a4","h7h5","a4a5","h5h4","a5a6","h4h3","a6b7","h3g2"
    };
    chess::Board pboard;
    for (const auto& text : promotion.moves) {
        auto move = chess::uci::uciToMove(pboard, text);
        assert(pboard.isLegal(move));
        pboard.makeMove(move);
    }
    promotion.fen = pboard.getFen();
    ChessSession pgame;
    assert(pgame.restore(promotion));
    pgame.tap_square(sq('b','7'));
    auto pending = pgame.tap_square(sq('a','8'));
    assert(pending.changed && !pending.moved);
    assert(pgame.pending() == ChessSession::Pending::PROMOTION);
    auto promoted = pgame.choose_promotion(chess::PieceType::KNIGHT);
    assert(promoted.moved);
    assert(pgame.uci_history().back() == "b7a8n");

    return 0;
}
