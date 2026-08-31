#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "../src/renderer.hpp"

using namespace inkchess;

int main() {
    (void)setenv("INKCHESS_ASSET_DIR", "/nonexistent/inkchess-assets", 1);

    KBConfig config;
    kb_config_defaults(&config);
    config.width = 1072;
    config.height = 1448;
    config.auto_clean = false;

    KBGame* const kb = kb_create(&config);
    assert(kb != nullptr);

    chess::Board board;
    UiModel model;
    model.board = &board;
    model.status = "WHITE TO MOVE";
    model.secondary = "STOCKFISH 1600";

    Renderer renderer(kb);
    renderer.draw_full(model, KB_REFRESH_CLEAN);

    const KBStats* stats = kb_stats(kb);
    assert(stats != nullptr);
    const std::uint64_t after_full = stats->presents;

    // An unchanged HUD with no dirty squares must not issue an e-ink update.
    renderer.draw_interaction(model, 0, KB_REFRESH_UI);
    assert(stats->presents == after_full);

    model.secondary = "ENGINE STARTING";
    renderer.draw_interaction(model, 0, KB_REFRESH_UI);
    assert(stats->presents == after_full + 1U);

    // Rossini portrait layout: board=(16,148,1040,1040), cell=130. Redrawing
    // a8 fills the top-left edge cell; the outer frame must remain black.
    constexpr int board_x = 16;
    constexpr int board_y = 148;
    constexpr int a8 = 56;
    const std::uint64_t dirty_before_square = stats->dirty_pixels;
    renderer.draw_interaction(model, 1ULL << a8, KB_REFRESH_UI);
    const std::uint64_t square_damage = stats->dirty_pixels - dirty_before_square;
    assert(square_damage < 4U * 130U * 130U);
    const KBCanvas* const canvas = kb_canvas(kb);
    assert(canvas != nullptr && canvas->pixels != nullptr);
    const std::size_t corner = static_cast<std::size_t>(board_y) *
                                   static_cast<std::size_t>(canvas->stride) +
                               static_cast<std::size_t>(board_x);
    assert(canvas->pixels[corner] == 0);

    kb_destroy(kb);
    return 0;
}
