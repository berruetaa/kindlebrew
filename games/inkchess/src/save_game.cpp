#include "save_game.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "../third_party/chess.hpp"

namespace inkchess {

namespace {

constexpr std::size_t kMaxSaveBytes = 64U * 1024U;
constexpr std::size_t kMaxMoves = 4096U;

std::uint32_t fnv1a(std::string_view bytes) {
    std::uint32_t hash = 2166136261U;
    for (const unsigned char c : bytes) {
        hash ^= c;
        hash *= 16777619U;
    }
    return hash;
}

void set_error(std::string* out, std::string message) {
    if (out) *out = std::move(message);
}

bool parse_int(std::string_view value, int min, int max, int* out) {
    if (value.empty() || value.size() > 16) return false;
    std::string copy(value);
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(copy.c_str(), &end, 10);
    if (errno != 0 || end == copy.c_str() || *end != '\0') return false;
    if (v < min || v > max) return false;
    *out = static_cast<int>(v);
    return true;
}

bool parse_hex32(std::string_view value, std::uint32_t* out) {
    if (value.size() != 8) return false;
    std::uint32_t result = 0;
    for (const char c : value) {
        result <<= 4U;
        if (c >= '0' && c <= '9')
            result |= static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
            result |= static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            result |= static_cast<std::uint32_t>(c - 'A' + 10);
        else
            return false;
    }
    *out = result;
    return true;
}

bool valid_uci_token(std::string_view move) {
    if (move.size() != 4 && move.size() != 5) return false;
    if (move[0] < 'a' || move[0] > 'h' ||
        move[2] < 'a' || move[2] > 'h' ||
        move[1] < '1' || move[1] > '8' ||
        move[3] < '1' || move[3] > '8') return false;
    if (move.size() == 5) {
        const char p = move[4];
        if (p != 'q' && p != 'r' && p != 'b' && p != 'n') return false;
    }
    return true;
}

std::optional<chess::Move> legal_uci(const chess::Board& board, std::string_view text) {
    if (!valid_uci_token(text)) return std::nullopt;
    const chess::Move candidate = chess::uci::uciToMove(board, std::string(text));
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);
    for (const auto& move : legal) {
        if (move == candidate) return move;
    }
    return std::nullopt;
}

}  // namespace

std::string encode_save(const SaveData& data) {
    std::ostringstream payload;
    payload << "mode=" << static_cast<int>(data.mode) << '\n';
    payload << "elo=" << data.elo << '\n';
    payload << "outcome=" << static_cast<int>(data.outcome) << '\n';
    payload << "moves=";
    for (std::size_t i = 0; i < data.moves.size(); ++i) {
        if (i) payload << ' ';
        payload << data.moves[i];
    }
    payload << '\n';
    payload << "fen=" << data.fen << '\n';

    const std::string body = payload.str();
    char checksum[9];
    std::snprintf(checksum, sizeof(checksum), "%08x", fnv1a(body));

    std::string result = "INKCHESS1\nchecksum=";
    result += checksum;
    result += '\n';
    result += body;
    return result;
}

std::optional<SaveData> decode_save(std::string_view bytes, std::string* error) {
    if (bytes.empty() || bytes.size() > kMaxSaveBytes) {
        set_error(error, "save size invalid");
        return std::nullopt;
    }

    constexpr std::string_view magic = "INKCHESS1\nchecksum=";
    if (bytes.rfind(magic, 0) != 0) {
        set_error(error, "save magic/version invalid");
        return std::nullopt;
    }

    const std::size_t checksum_end = bytes.find('\n', magic.size());
    if (checksum_end == std::string_view::npos) {
        set_error(error, "save checksum line missing");
        return std::nullopt;
    }

    std::uint32_t stored = 0;
    if (!parse_hex32(bytes.substr(magic.size(), checksum_end - magic.size()), &stored)) {
        set_error(error, "save checksum invalid");
        return std::nullopt;
    }

    const std::string_view body = bytes.substr(checksum_end + 1);
    if (fnv1a(body) != stored) {
        set_error(error, "save checksum mismatch");
        return std::nullopt;
    }

    SaveData result;
    bool have_mode = false;
    bool have_elo = false;
    bool have_outcome = false;
    bool have_moves = false;
    bool have_fen = false;

    std::size_t pos = 0;
    while (pos < body.size()) {
        const std::size_t nl = body.find('\n', pos);
        const std::size_t end = nl == std::string_view::npos ? body.size() : nl;
        const std::string_view line = body.substr(pos, end - pos);
        pos = nl == std::string_view::npos ? body.size() : nl + 1;
        if (line.empty()) continue;

        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            set_error(error, "save line malformed");
            return std::nullopt;
        }
        const std::string_view key = line.substr(0, eq);
        const std::string_view value = line.substr(eq + 1);

        int parsed = 0;
        if (key == "mode") {
            if (have_mode || !parse_int(value, 0, 2, &parsed)) {
                set_error(error, "save mode invalid");
                return std::nullopt;
            }
            result.mode = static_cast<PlayMode>(parsed);
            have_mode = true;
        } else if (key == "elo") {
            if (have_elo || !parse_int(value, 1320, 3190, &parsed)) {
                set_error(error, "save elo invalid");
                return std::nullopt;
            }
            result.elo = parsed;
            have_elo = true;
        } else if (key == "outcome") {
            if (have_outcome || !parse_int(value, 0, 3, &parsed)) {
                set_error(error, "save outcome invalid");
                return std::nullopt;
            }
            result.outcome = static_cast<OutcomeOverride>(parsed);
            have_outcome = true;
        } else if (key == "moves") {
            if (have_moves) {
                set_error(error, "duplicate moves");
                return std::nullopt;
            }
            std::istringstream in{std::string(value)};
            std::string move;
            while (in >> move) {
                if (!valid_uci_token(move) || result.moves.size() >= kMaxMoves) {
                    set_error(error, "save move list invalid");
                    return std::nullopt;
                }
                result.moves.push_back(std::move(move));
            }
            have_moves = true;
        } else if (key == "fen") {
            if (have_fen || value.empty() || value.size() > 128) {
                set_error(error, "save fen invalid");
                return std::nullopt;
            }
            result.fen = std::string(value);
            have_fen = true;
        } else {
            set_error(error, "unknown save field");
            return std::nullopt;
        }
    }

    if (!have_mode || !have_elo || !have_outcome || !have_moves || !have_fen) {
        set_error(error, "save fields incomplete");
        return std::nullopt;
    }

    chess::Board board;
    for (const auto& text : result.moves) {
        const auto move = legal_uci(board, text);
        if (!move) {
            set_error(error, "save contains illegal move");
            return std::nullopt;
        }
        board.makeMove(*move);
    }

    if (board.getFen() != result.fen) {
        set_error(error, "save FEN/history mismatch");
        return std::nullopt;
    }

    return result;
}

}  // namespace inkchess
