#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace inkchess {

enum class PlayMode : int {
    HUMAN_WHITE = 0,
    HUMAN_BLACK = 1,
    LOCAL_TWO_PLAYER = 2
};

enum class OutcomeOverride : int {
    NONE = 0,
    DRAW_CLAIM = 1,
    WHITE_WIN_RESIGN = 2,
    BLACK_WIN_RESIGN = 3
};

struct SaveData {
    PlayMode mode = PlayMode::HUMAN_WHITE;
    int elo = 1600;
    OutcomeOverride outcome = OutcomeOverride::NONE;
    std::vector<std::string> moves;
    std::string fen;
};

[[nodiscard]] std::string encode_save(const SaveData& data);
[[nodiscard]] std::optional<SaveData> decode_save(std::string_view bytes,
                                                  std::string* error = nullptr);

}  // namespace inkchess
