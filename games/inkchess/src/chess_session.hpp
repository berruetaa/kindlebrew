#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../third_party/chess.hpp"
#include "game_rules.hpp"
#include "save_game.hpp"

namespace inkchess {

class ChessSession {
   public:
    enum class Pending {
        NONE = 0,
        PROMOTION,
        CLAIM_INTENDED
    };

    struct TapResult {
        bool changed = false;
        bool moved = false;
        std::uint64_t dirty_squares = 0;
    };

    ChessSession();

    void reset(PlayMode mode, int elo);
    bool restore(const SaveData& save);
    [[nodiscard]] SaveData save_data() const;

    [[nodiscard]] const chess::Board& board() const noexcept { return board_; }
    [[nodiscard]] PlayMode mode() const noexcept { return mode_; }
    [[nodiscard]] int elo() const noexcept { return elo_; }
    [[nodiscard]] OutcomeOverride outcome_override() const noexcept { return outcome_; }
    [[nodiscard]] const std::vector<std::string>& uci_history() const noexcept { return uci_history_; }
    [[nodiscard]] const std::string& last_san() const noexcept { return last_san_; }

    [[nodiscard]] bool human_turn() const noexcept;
    [[nodiscard]] bool engine_turn() const noexcept;
    [[nodiscard]] bool ended() const;
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_claim_current() const;
    [[nodiscard]] bool engine_should_claim() const;
    [[nodiscard]] bool engine_can_claim_with_move(std::string_view uci) const;

    [[nodiscard]] int selected_square() const noexcept { return selected_square_; }
    [[nodiscard]] std::uint64_t legal_target_mask() const noexcept { return target_mask_; }
    [[nodiscard]] std::uint64_t last_move_mask() const noexcept { return last_move_mask_; }
    [[nodiscard]] Pending pending() const noexcept { return pending_; }

    TapResult tap_square(int square);
    TapResult choose_promotion(chess::PieceType type);
    TapResult play_pending_claim_move();
    bool claim_pending_draw();
    bool claim_current_draw();
    bool claim_engine_current_draw();
    bool claim_engine_intended_draw(std::string_view uci);

    // Engine moves are always independently checked against current legal moves.
    bool apply_engine_move(std::string_view uci, std::uint64_t* dirty_squares = nullptr);

    // Call only after an active engine search has been stopped/consumed.
    std::uint64_t undo();
    void resign();

    [[nodiscard]] TerminalState automatic_terminal() const;
    [[nodiscard]] std::string status_text() const;
    [[nodiscard]] std::string secondary_text(bool engine_available, bool engine_thinking) const;

   private:
    [[nodiscard]] std::optional<chess::Move> legal_uci(std::string_view text) const;
    [[nodiscard]] std::vector<chess::Move> moves_from_to(int from, int to) const;
    [[nodiscard]] std::uint64_t selection_mask() const noexcept;
    [[nodiscard]] std::uint64_t compute_piece_diff(const chess::Board& before,
                                                   const chess::Board& after) const;
    [[nodiscard]] std::uint64_t move_display_mask(std::string_view uci) const;
    [[nodiscard]] bool piece_is_current_side(int square) const;
    void select(int square);
    TapResult prepare_move(const chess::Move& move);
    std::uint64_t commit_move(const chess::Move& move);
    void clear_pending();
    void update_last_metadata();

    chess::Board board_;
    PlayMode mode_ = PlayMode::HUMAN_WHITE;
    int elo_ = 1600;
    OutcomeOverride outcome_ = OutcomeOverride::NONE;

    std::vector<chess::Move> move_history_;
    std::vector<std::string> uci_history_;
    std::vector<std::string> san_history_;

    int selected_square_ = -1;
    std::uint64_t target_mask_ = 0;
    std::uint64_t last_move_mask_ = 0;
    std::string last_san_;

    Pending pending_ = Pending::NONE;
    std::vector<chess::Move> promotion_moves_;
    std::optional<chess::Move> pending_move_;
};

}  // namespace inkchess
