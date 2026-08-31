#include <cassert>
#include <fstream>
#include <string>

#include <cstdlib>
#include <unistd.h>

#include "../src/save_game.hpp"
#include "../src/chess_vendor.hpp"

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

    // A syntactically legal move list may not continue after automatic
    // fivefold repetition.
    SaveData continued = original;
    continued.outcome = OutcomeOverride::NONE;
    continued.moves.clear();
    for (int cycle = 0; cycle < 4; ++cycle) {
        continued.moves.insert(continued.moves.end(),
                               {"g1f3", "g8f6", "f3g1", "f6g8"});
    }
    continued.moves.push_back("e2e4");
    chess::Board continued_board;
    for (const auto& text : continued.moves) {
        const auto move = chess::uci::uciToMove(continued_board, text);
        assert(continued_board.isLegal(move));
        continued_board.makeMove(move);
    }
    continued.fen = continued_board.getFen();
    assert(!decode_save(encode_save(continued), &error).has_value());

    // Each corrupt save gets a distinct evidence path; the second quarantine
    // must not overwrite the first one even within the same second/process.
    char temp_dir[] = "/tmp/inkchess-save-XXXXXX";
    assert(mkdtemp(temp_dir) != nullptr);
    const std::string corrupt_path = std::string(temp_dir) + "/save-v1.txt";
    {
        std::ofstream out(corrupt_path, std::ios::binary);
        out << "first-corrupt";
        assert(out.good());
    }
    std::string bad_one;
    assert(quarantine_save_file(corrupt_path, &bad_one, &error));
    {
        std::ofstream out(corrupt_path, std::ios::binary);
        out << "second-corrupt";
        assert(out.good());
    }
    std::string bad_two;
    assert(quarantine_save_file(corrupt_path, &bad_two, &error));
    assert(bad_one != bad_two);
    std::ifstream first(bad_one, std::ios::binary);
    std::ifstream second(bad_two, std::ios::binary);
    std::string first_bytes;
    std::string second_bytes;
    first >> first_bytes;
    second >> second_bytes;
    assert(first_bytes == "first-corrupt");
    assert(second_bytes == "second-corrupt");
    first.close();
    second.close();
    assert(unlink(bad_one.c_str()) == 0);
    assert(unlink(bad_two.c_str()) == 0);
    assert(rmdir(temp_dir) == 0);

    return 0;
}
