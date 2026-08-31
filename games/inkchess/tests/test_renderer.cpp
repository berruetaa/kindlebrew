#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

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

    Renderer missing_assets(kb);
    assert(!missing_assets.assets_ready());

    char asset_dir[] = "/tmp/inkchess-renderer-XXXXXX";
    assert(mkdtemp(asset_dir) != nullptr);
    constexpr std::array<const char*, 12> names = {
        "whitePawn.r8a8", "whiteKnight.r8a8", "whiteBishop.r8a8",
        "whiteRook.r8a8", "whiteQueen.r8a8", "whiteKing.r8a8",
        "blackPawn.r8a8", "blackKnight.r8a8", "blackBishop.r8a8",
        "blackRook.r8a8", "blackQueen.r8a8", "blackKing.r8a8"};
    const std::vector<char> fixture(32768, static_cast<char>(0x7f));
    for (const char* name : names) {
        std::ofstream out(std::string(asset_dir) + "/" + name, std::ios::binary);
        out.write(fixture.data(), static_cast<std::streamsize>(fixture.size()));
        assert(out.good());
    }
    (void)setenv("INKCHESS_ASSET_DIR", asset_dir, 1);

    chess::Board board;
    UiModel model;
    model.board = &board;
    model.status = "WHITE TO MOVE";
    model.secondary = "STOCKFISH 1600";

    Renderer renderer(kb);
    assert(renderer.assets_ready());
    renderer.draw_full(model, KB_REFRESH_CLEAN);
    assert(renderer.healthy());

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
    for (const char* name : names) {
        assert(unlink((std::string(asset_dir) + "/" + name).c_str()) == 0);
    }
    assert(rmdir(asset_dir) == 0);
    return 0;
}
