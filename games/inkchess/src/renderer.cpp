#include "renderer.hpp"

#include <algorithm>
#include <cstdint>

namespace inkchess {

namespace {

constexpr std::array<const char*, 6> kPieceLetters = {"P", "N", "B", "R", "Q", "K"};

}  // namespace

Renderer::Renderer(KBGame* kb) : kb_(kb) {
    relayout();
}

bool Renderer::contains(KBRect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

int Renderer::fit_text(const std::string& text, int max_w, int max_h, int max_scale) {
    int scale = std::max(1, max_scale);
    while (scale > 1) {
        const KBRect m = kb_measure_text8(text.c_str(), scale);
        if (m.w <= max_w && m.h <= max_h) break;
        --scale;
    }
    return scale;
}

void Renderer::draw_centered(KBRect r, const std::string& text, int max_scale,
                             std::uint8_t fg, int bg) {
    if (text.empty()) return;
    const int scale = fit_text(text, std::max(1, r.w - 8), std::max(1, r.h - 8), max_scale);
    const KBRect m = kb_measure_text8(text.c_str(), scale);
    kb_draw_text8(kb_, r.x + (r.w - m.w) / 2, r.y + (r.h - m.h) / 2,
                  text.c_str(), scale, fg, bg);
}

void Renderer::relayout() {
    const KBCanvas* c = kb_canvas(kb_);
    l_ = {};
    l_.width = c->width;
    l_.height = c->height;
    l_.margin = std::max(10, std::min(c->width, c->height) / 70);

    const int header_h = std::max(70, std::min(118, c->height / 11));
    const int min_footer = std::max(135, std::min(230, c->height / 6));
    const int max_board_w = c->width - 2 * l_.margin;
    const int max_board_h = c->height - header_h - min_footer - 3 * l_.margin;
    int cell = std::max(24, std::min(max_board_w, max_board_h) / 8);
    const int board_side = cell * 8;

    l_.header = {l_.margin, l_.margin, c->width - 2 * l_.margin, header_h};
    l_.board = {(c->width - board_side) / 2,
                l_.header.y + l_.header.h + l_.margin,
                board_side, board_side};
    l_.cell = cell;
    l_.footer = {l_.margin, l_.board.y + l_.board.h + l_.margin,
                 c->width - 2 * l_.margin,
                 c->height - (l_.board.y + l_.board.h + 2 * l_.margin)};

    const int gap = std::max(6, l_.margin / 2);
    const int button_h = std::max(48, std::min(84, l_.footer.h / 2));
    const int button_w = (l_.footer.w - gap * 4) / 5;
    const int button_y = l_.footer.y + l_.footer.h - button_h;
    for (int i = 0; i < 5; ++i) {
        l_.buttons[static_cast<std::size_t>(i)] = {
            l_.footer.x + i * (button_w + gap), button_y, button_w, button_h};
    }

    const int modal_w = std::min(l_.width - 4 * l_.margin, std::max(360, l_.board.w * 4 / 5));
    const int modal_h = std::min(l_.height - 4 * l_.margin, std::max(420, l_.board.h * 3 / 4));
    l_.modal = {(l_.width - modal_w) / 2, (l_.height - modal_h) / 2, modal_w, modal_h};

    const int option_gap = std::max(8, l_.margin);
    const int option_top = l_.modal.y + std::max(70, l_.modal.h / 6);
    const int available = l_.modal.y + l_.modal.h - option_top - l_.margin;
    const int option_h = std::max(48, (available - option_gap * 4) / 5);
    for (int i = 0; i < 5; ++i) {
        l_.modal_options[static_cast<std::size_t>(i)] = {
            l_.modal.x + l_.margin,
            option_top + i * (option_h + option_gap),
            l_.modal.w - 2 * l_.margin,
            option_h};
    }
}

bool Renderer::flipped(const UiModel& model) const {
    return model.mode == PlayMode::HUMAN_BLACK;
}

int Renderer::display_to_square(bool flip, int row, int col) {
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return -1;
    const int rank = flip ? row : 7 - row;
    const int file = flip ? 7 - col : col;
    return rank * 8 + file;
}

void Renderer::square_to_display(bool flip, int square, int* row, int* col) {
    const int rank = square / 8;
    const int file = square % 8;
    *row = flip ? rank : 7 - rank;
    *col = flip ? 7 - file : file;
}

KBRect Renderer::square_rect(const UiModel& model, int square) const {
    int row = 0;
    int col = 0;
    square_to_display(flipped(model), square, &row, &col);
    return {l_.board.x + col * l_.cell, l_.board.y + row * l_.cell, l_.cell, l_.cell};
}

void Renderer::draw_piece(KBRect cell, chess::Piece piece) {
    if (piece == chess::Piece::NONE) return;

    const int cx = cell.x + cell.w / 2;
    const int cy = cell.y + cell.h / 2;
    const int radius = std::max(8, cell.w * 31 / 100);
    const bool white = piece.color() == chess::Color::WHITE;

    kb_fill_circle(kb_, cx, cy, radius, 0);
    if (white) {
        kb_fill_circle(kb_, cx, cy, std::max(2, radius - std::max(3, cell.w / 32)), 248);
    }

    const int type = static_cast<int>(piece.type());
    if (type < 0 || type >= static_cast<int>(kPieceLetters.size())) return;
    const std::string letter = kPieceLetters[static_cast<std::size_t>(type)];
    const int scale = std::max(2, std::min(10, cell.w / 14));
    const KBRect m = kb_measure_text8(letter.c_str(), scale);
    kb_draw_text8(kb_, cx - m.w / 2, cy - m.h / 2, letter.c_str(), scale,
                  white ? 0 : 255, -1);
}

void Renderer::draw_square(const UiModel& model, int square) {
    if (!model.board || square < 0 || square >= 64) return;
    KBRect r = square_rect(model, square);
    const int rank = square / 8;
    const int file = square % 8;
    const std::uint8_t bg = ((rank + file) & 1) ? 205 : 247;

    kb_fill_rect(kb_, r, bg);

    if (model.last_move_mask & (1ULL << square)) {
        const int inset = std::max(3, l_.cell / 22);
        kb_draw_rect(kb_, {r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset},
                     std::max(2, l_.cell / 35), 90);
    }

    const chess::Piece piece = model.board->at(chess::Square(square));
    draw_piece(r, piece);

    if (model.legal_target_mask & (1ULL << square)) {
        if (piece == chess::Piece::NONE) {
            kb_fill_circle(kb_, r.x + r.w / 2, r.y + r.h / 2,
                           std::max(4, l_.cell / 15), 0);
        } else {
            const int inset = std::max(6, l_.cell / 11);
            kb_draw_rect(kb_, {r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset},
                         std::max(3, l_.cell / 24), 0);
        }
    }

    if (model.selected_square == square) {
        kb_draw_rect(kb_, r, std::max(4, l_.cell / 20), 0);
    }

    if (model.board->inCheck() &&
        piece != chess::Piece::NONE &&
        piece.type() == chess::PieceType::KING &&
        piece.color() == model.board->sideToMove()) {
        const int inset = std::max(10, l_.cell / 7);
        kb_draw_rect(kb_, {r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset},
                     std::max(3, l_.cell / 28), 0);
    }
}

void Renderer::draw_board(const UiModel& model) {
    for (int sq = 0; sq < 64; ++sq) draw_square(model, sq);
    kb_draw_rect(kb_, l_.board, std::max(3, l_.cell / 28), 0);
}

void Renderer::draw_header(const UiModel& model) {
    kb_fill_rect(kb_, l_.header, 255);

    KBRect title = {l_.header.x, l_.header.y, l_.header.w / 3, l_.header.h};
    draw_centered(title, "INK CHESS", 4, 0, -1);

    KBRect status = {l_.header.x + l_.header.w / 3, l_.header.y,
                     l_.header.w * 2 / 3, l_.header.h * 3 / 5};
    draw_centered(status, model.status, 3, 0, -1);

    KBRect secondary = {status.x, l_.header.y + l_.header.h * 3 / 5,
                        status.w, l_.header.h * 2 / 5};
    draw_centered(secondary, model.secondary, 2, 70, -1);

    kb_draw_line(kb_, l_.header.x, l_.header.y + l_.header.h - 2,
                 l_.header.x + l_.header.w, l_.header.y + l_.header.h - 2, 2, 0);
}

void Renderer::draw_button(KBRect r, const std::string& label, bool enabled) {
    kb_fill_rect(kb_, r, enabled ? 255 : 235);
    kb_draw_rect(kb_, r, 2, enabled ? 0 : 140);
    draw_centered(r, label, 2, enabled ? 0 : 140, -1);
}

void Renderer::draw_footer(const UiModel& model) {
    kb_fill_rect(kb_, l_.footer, 255);

    if (model.engine_thinking) {
        KBRect info = {l_.footer.x, l_.footer.y, l_.footer.w, l_.footer.h / 3};
        draw_centered(info, "STOCKFISH THINKING", 2, 0, -1);
    } else if (!model.engine_available && model.mode != PlayMode::LOCAL_TWO_PLAYER) {
        KBRect info = {l_.footer.x, l_.footer.y, l_.footer.w, l_.footer.h / 3};
        draw_centered(info, "ENGINE UNAVAILABLE - LOCAL PLAY STILL AVAILABLE", 2, 0, -1);
    }

    draw_button(l_.buttons[0], "NEW", true);
    draw_button(l_.buttons[1], "UNDO", model.can_undo);
    draw_button(l_.buttons[2], "DRAW", model.can_claim_draw);
    draw_button(l_.buttons[3], "RESIGN", true);
    draw_button(l_.buttons[4], "EXIT", true);
}

void Renderer::draw_overlay(const UiModel& model) {
    if (model.overlay == Overlay::NONE) return;

    kb_fill_rect(kb_, l_.modal, 255);
    kb_draw_rect(kb_, l_.modal, std::max(4, l_.margin / 2), 0);

    KBRect title = {l_.modal.x + l_.margin, l_.modal.y + l_.margin,
                    l_.modal.w - 2 * l_.margin, std::max(50, l_.modal.h / 8)};
    draw_centered(title, model.overlay_title, 3, 0, -1);

    for (int i = 0; i < model.overlay_option_count && i < 5; ++i) {
        draw_button(l_.modal_options[static_cast<std::size_t>(i)],
                    model.overlay_options[static_cast<std::size_t>(i)], true);
    }
}

void Renderer::draw_full(const UiModel& model, KBRefreshMode refresh) {
    kb_clear(kb_, 255);
    draw_header(model);
    draw_board(model);
    draw_footer(model);
    draw_overlay(model);
    (void)kb_present(kb_, refresh);
}

void Renderer::draw_interaction(const UiModel& model, std::uint64_t square_mask,
                                KBRefreshMode refresh) {
    draw_header(model);
    for (int sq = 0; sq < 64; ++sq) {
        if (square_mask & (1ULL << sq)) draw_square(model, sq);
    }
    draw_footer(model);
    if (model.overlay != Overlay::NONE) draw_overlay(model);
    (void)kb_present(kb_, refresh);
}

int Renderer::square_at(const UiModel& model, int x, int y) const {
    if (!contains(l_.board, x, y)) return -1;
    const int col = (x - l_.board.x) / l_.cell;
    const int row = (y - l_.board.y) / l_.cell;
    return display_to_square(flipped(model), row, col);
}

UiButton Renderer::button_at(int x, int y) const {
    for (int i = 0; i < 5; ++i) {
        if (contains(l_.buttons[static_cast<std::size_t>(i)], x, y)) {
            return static_cast<UiButton>(i + 1);
        }
    }
    return UiButton::NONE;
}

int Renderer::overlay_option_at(const UiModel& model, int x, int y) const {
    if (model.overlay == Overlay::NONE) return -1;
    for (int i = 0; i < model.overlay_option_count && i < 5; ++i) {
        if (contains(l_.modal_options[static_cast<std::size_t>(i)], x, y)) return i;
    }
    return -1;
}

}  // namespace inkchess
