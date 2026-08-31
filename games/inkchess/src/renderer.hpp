#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "kbgame.h"
#include "chess_vendor.hpp"
#include "save_game.hpp"

namespace inkchess {

enum class Overlay {
    NONE = 0,
    NEW_MODE,
    NEW_LEVEL,
    PROMOTION,
    CLAIM_INTENDED,
    RESIGN_CONFIRM
};

enum class UiButton {
    NONE = 0,
    NEW_GAME,
    UNDO,
    CLAIM_DRAW,
    RESIGN,
    EXIT
};

struct UiModel {
    const chess::Board* board = nullptr;
    PlayMode mode = PlayMode::HUMAN_WHITE;
    int selected_square = -1;
    std::uint64_t legal_target_mask = 0;
    std::uint64_t last_move_mask = 0;
    bool can_undo = false;
    bool can_claim_draw = false;
    bool can_resign = false;
    bool engine_thinking = false;
    bool engine_available = true;
    std::string status;
    std::string secondary;

    Overlay overlay = Overlay::NONE;
    std::array<std::string, 5> overlay_options{};
    int overlay_option_count = 0;
    std::string overlay_title;
};

class Renderer {
   public:
    explicit Renderer(KBGame* kb);

    void relayout();
    void draw_full(const UiModel& model, KBRefreshMode refresh);
    void draw_interaction(const UiModel& model, std::uint64_t square_mask,
                          KBRefreshMode refresh);

    [[nodiscard]] int square_at(const UiModel& model, int x, int y) const;
    [[nodiscard]] UiButton button_at(int x, int y) const;
    [[nodiscard]] int overlay_option_at(const UiModel& model, int x, int y) const;

   private:
    struct PieceBitmap {
        bool loaded = false;
        std::vector<std::uint8_t> gray;
        std::vector<std::uint8_t> alpha;
        std::vector<std::uint8_t> scaled_gray;
        std::vector<std::uint8_t> scaled_alpha;
    };

    struct Layout {
        int width = 0;
        int height = 0;
        int margin = 0;
        KBRect header{};
        KBRect board{};
        KBRect footer{};
        int cell = 0;
        std::array<KBRect, 5> buttons{};
        KBRect modal{};
        std::array<KBRect, 5> modal_options{};
    };

    static bool contains(KBRect r, int x, int y);
    static int fit_text(const std::string& text, int max_w, int max_h, int max_scale);
    static int display_to_square(bool flip, int row, int col);
    static void square_to_display(bool flip, int square, int* row, int* col);
    static bool same_header(const UiModel& a, const UiModel& b);
    static bool same_footer(const UiModel& a, const UiModel& b);

    bool flipped(const UiModel& model) const;
    KBRect square_rect(const UiModel& model, int square) const;
    void draw_header(const UiModel& model);
    void draw_footer(const UiModel& model);
    void draw_board(const UiModel& model);
    void draw_square(const UiModel& model, int square);
    void draw_piece(KBRect cell, chess::Piece piece);
    void draw_button(KBRect r, const std::string& label, bool enabled);
    void draw_overlay(const UiModel& model);
    void draw_centered(KBRect r, const std::string& text, int max_scale,
                       std::uint8_t fg, int bg);
    void load_piece_assets();
    void rebuild_piece_cache();
    [[nodiscard]] static int piece_asset_index(chess::Piece piece);

    KBGame* kb_ = nullptr;
    Layout l_{};
    std::array<PieceBitmap, 12> pieces_{};
    std::string asset_dir_;
    bool assets_attempted_ = false;
    int piece_px_ = 0;
    UiModel cached_model_{};
    bool model_cache_valid_ = false;
};

}  // namespace inkchess
