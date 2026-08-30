#include <cassert>
#include <initializer_list>
#include <string>

#include "../src/chess_session.hpp"

using namespace inkchess;

static int sq(char file, char rank) {
    return (rank - '1') * 8 + (file - 'a');
}

static bool restore_moves(ChessSession& game, PlayMode mode,
                          std::initializer_list<const char*> moves) {
    SaveData save;
    save.mode = mode;
    save.elo = 1600;
    save.outcome = OutcomeOverride::NONE;

    chess::Board board;
    for (const char* text : moves) {
        const chess::Move move = chess::uci::uciToMove(board, text);
        if (!board.isLegal(move)) return false;
        save.moves.emplace_back(text);
        board.makeMove(move);
    }
    save.fen = board.getFen();
    return game.restore(save);
}

int main() {
    ChessSession game;
    game.reset(PlayMode::HUMAN_WHITE, 1600);

    auto a = game.tap_square(sq('e', '2'));
    assert(a.changed && !a.moved);
    assert(game.selected_square() == sq('e', '2'));
    assert(game.legal_target_mask() & (1ULL << sq('e', '4')));

    // A second tap on the selected piece cancels selection. On Kindle this
    // arrives as KB_EVENT_DOUBLE_TAP when the taps are close together.
    const std::string start = game.board().getFen();
    auto cancel = game.tap_square(sq('e', '2'));
    assert(cancel.changed && !cancel.moved);
    assert(game.selected_square() == -1);
    assert(game.legal_target_mask() == 0);
    assert(game.board().getFen() == start);

    // Illegal target clears selection without mutating the board.
    game.tap_square(sq('e', '2'));
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

    // A check marker lives on an unchanged king square. It must be dirty both
    // when a line check appears and when a blocking move removes it.
    ChessSession checked;
    assert(restore_moves(checked, PlayMode::LOCAL_TWO_PLAYER, {"e2e4", "f7f5"}));
    checked.tap_square(sq('d', '1'));
    const auto give_check = checked.tap_square(sq('h', '5'));
    assert(give_check.moved);
    assert(give_check.dirty_squares & (1ULL << sq('e', '8')));
    checked.tap_square(sq('g', '7'));
    const auto block_check = checked.tap_square(sq('g', '6'));
    assert(block_check.moved);
    assert(block_check.dirty_squares & (1ULL << sq('e', '8')));
    const std::uint64_t undo_check = checked.undo();
    assert(undo_check & (1ULL << sq('e', '8')));

    // If Stockfish delivered mate, undo two plies so control returns to the
    // human. If the human delivered mate, only their mating ply is removed.
    ChessSession engine_mate;
    assert(restore_moves(engine_mate, PlayMode::HUMAN_WHITE,
                         {"f2f3", "e7e5", "g2g4", "d8h4"}));
    assert(engine_mate.ended() && engine_mate.can_undo());
    assert(engine_mate.undo() != 0);
    assert(engine_mate.uci_history().size() == 2);
    assert(!engine_mate.ended() && engine_mate.human_turn());

    ChessSession human_mate;
    assert(restore_moves(human_mate, PlayMode::HUMAN_WHITE,
                         {"e2e4", "e7e5", "d1h5", "b8c6", "f1c4", "g8f6", "h5f7"}));
    assert(human_mate.ended() && human_mate.can_undo());
    assert(human_mate.undo() != 0);
    assert(human_mate.uci_history().size() == 6);
    assert(!human_mate.ended() && human_mate.human_turn());

    // Resignation and accepted draw claims do not create a move. Disabling
    // undo avoids silently deleting an unrelated previous ply.
    ChessSession resigned;
    assert(restore_moves(resigned, PlayMode::HUMAN_WHITE, {"e2e4", "e7e5"}));
    const std::string before_resign = resigned.board().getFen();
    resigned.resign();
    assert(resigned.ended() && !resigned.can_undo());
    assert(resigned.undo() == 0);
    assert(resigned.board().getFen() == before_resign);

    return 0;
}
