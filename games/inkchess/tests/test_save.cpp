#include <cassert>
#include <string>

#include "../src/save_game.hpp"
#include "../third_party/chess.hpp"

using namespace inkchess;

static SaveData sample() {
    SaveData data;
    data.mode = PlayMode::HUMAN_BLACK;
    data.elo = 2000;
    data.outcome = OutcomeOverride::NONE;
    data.moves = {"e2e4", "e7e5", "g1f3"};

    chess::Board board;
    for (const auto& text : data.moves) {
        const auto move = chess::uci::uciToMove(board, text);
        assert(board.isLegal(move));
        board.makeMove(move);
    }
    data.fen = board.getFen();
    return data;
}

int main() {
    const SaveData original = sample();
    const std::string bytes = encode_save(original);
    std::string error;
    auto decoded = decode_save(bytes, &error);
    assert(decoded.has_value());
    assert(decoded->mode == original.mode);
    assert(decoded->elo == original.elo);
    assert(decoded->outcome == original.outcome);
    assert(decoded->moves == original.moves);
    assert(decoded->fen == original.fen);

    std::string corrupted = bytes;
    corrupted.back() = corrupted.back() == 'x' ? 'y' : 'x';
    assert(!decode_save(corrupted, &error).has_value());

    SaveData mismatch = original;
    mismatch.fen = chess::constants::STARTPOS;
    const std::string mismatch_bytes = encode_save(mismatch);
    assert(!decode_save(mismatch_bytes, &error).has_value());

    SaveData illegal = original;
    illegal.moves = {"e2e5"};
    illegal.fen = chess::constants::STARTPOS;
    assert(!decode_save(encode_save(illegal), &error).has_value());

    return 0;
}
