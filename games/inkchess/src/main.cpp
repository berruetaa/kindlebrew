#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <unistd.h>

#include "kbgame.h"
#include "chess_session.hpp"
#include "renderer.hpp"
#include "save_game.hpp"
#include "uci_engine.hpp"

namespace inkchess {

namespace {

constexpr int TIMER_SETTLE = 100;
constexpr int TIMER_ENGINE_GUARD = 101;
constexpr int ENGINE_FD_ID = 200;
constexpr unsigned SETTLE_MS = 520;
constexpr unsigned ENGINE_HANDSHAKE_TIMEOUT_MS = 7000;
constexpr unsigned ENGINE_SYNC_TIMEOUT_MS = 5000;
constexpr unsigned ENGINE_STOP_TIMEOUT_MS = 5000;

unsigned think_time_ms(int elo) {
    if (elo <= 1400) return 250;
    if (elo <= 1700) return 450;
    if (elo <= 2100) return 700;
    if (elo <= 2600) return 1000;
    return 1400;
}

}  // namespace

class App {
   public:
    App(KBGame* kb, std::string engine_path)
        : kb_(kb), renderer_(kb), engine_path_(std::move(engine_path)) {}

    ~App() {
        stop_engine();
    }

    bool initialize() {
        if (kb_data_path(kb_, "save-v1.txt", save_path_, sizeof(save_path_)) != 0) {
            runtime_message_ = "SAVE PATH ERROR";
            return false;
        }

        bool restored = false;
        size_t size = 0;
        void* raw = kb_load_file(save_path_, &size);
        if (raw) {
            std::string error;
            const auto decoded = decode_save(
                std::string_view(static_cast<const char*>(raw), size), &error);
            kb_free(raw);

            if (decoded && session_.restore(*decoded)) {
                restored = true;
            } else {
                quarantine_bad_save();
                runtime_message_ = "BAD SAVE RESET";
            }
        }

        if (!restored) {
            session_.reset(PlayMode::HUMAN_WHITE, 1600);
            save_now();
        }

        if (session_.mode() != PlayMode::LOCAL_TWO_PLAYER && !session_.ended()) {
            start_engine();
        }

        renderer_.draw_full(model(), KB_REFRESH_CLEAN);
        service_engine();
        return true;
    }

    bool running() const noexcept { return running_; }

    void on_event(const KBEvent& ev) {
        switch (ev.type) {
            case KB_EVENT_TAP:
                handle_tap(ev.x, ev.y);
                break;

            case KB_EVENT_FD:
                if (ev.id == ENGINE_FD_ID) handle_engine_fd(static_cast<unsigned>(ev.value));
                break;

            case KB_EVENT_TIMER:
                handle_timer(ev.id);
                break;

            case KB_EVENT_SUSPEND:
                handle_suspend();
                break;

            case KB_EVENT_RESUME:
                handle_resume();
                break;

            case KB_EVENT_RESIZE:
                renderer_.relayout();
                settle_mask_ = 0;
                (void)kb_timer_cancel(kb_, TIMER_SETTLE);
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                break;

            case KB_EVENT_KEY:
                if (ev.value == 1 && (ev.key == 1 || ev.key == 102)) running_ = false;
                break;

            case KB_EVENT_QUIT:
                running_ = false;
                break;

            default:
                break;
        }
    }

    void shutdown() {
        save_now();
        stop_engine();
    }

   private:
    UiModel model() const {
        UiModel m;
        m.board = &session_.board();
        m.mode = session_.mode();
        m.selected_square = session_.selected_square();
        m.legal_target_mask = session_.legal_target_mask();
        m.last_move_mask = session_.last_move_mask();
        m.can_undo = session_.can_undo() && !pending_undo_;
        m.can_claim_draw = session_.can_claim_current();
        m.can_resign = !session_.ended() && session_.human_turn();
        m.engine_thinking = engine_.state() == UciEngine::State::SEARCHING ||
                            engine_.state() == UciEngine::State::STOPPING;
        m.engine_available = session_.mode() == PlayMode::LOCAL_TWO_PLAYER || engine_.alive();
        m.status = pending_undo_ ? "UNDO PENDING" : session_.status_text();

        if (!runtime_message_.empty()) {
            m.secondary = runtime_message_;
        } else if (session_.mode() != PlayMode::LOCAL_TWO_PLAYER && engine_.alive() && !engine_synced_) {
            m.secondary = "ENGINE STARTING";
        } else {
            m.secondary = session_.secondary_text(m.engine_available, m.engine_thinking);
        }

        m.overlay = active_overlay();
        switch (m.overlay) {
            case Overlay::NEW_MODE:
                m.overlay_title = "NEW GAME - PLAY AS";
                m.overlay_options = {"WHITE", "BLACK", "2 PLAYERS", "", ""};
                m.overlay_option_count = 3;
                break;
            case Overlay::NEW_LEVEL:
                m.overlay_title = "STOCKFISH LEVEL";
                m.overlay_options = {"EASY 1320", "CLUB 1600", "STRONG 2000",
                                     "EXPERT 2500", "MAX 3190"};
                m.overlay_option_count = 5;
                break;
            case Overlay::PROMOTION:
                m.overlay_title = "PROMOTE TO";
                m.overlay_options = {"QUEEN", "ROOK", "BISHOP", "KNIGHT", ""};
                m.overlay_option_count = 4;
                break;
            case Overlay::CLAIM_INTENDED:
                m.overlay_title = "THIS MOVE CAN CLAIM A DRAW";
                m.overlay_options = {"CLAIM DRAW", "PLAY MOVE", "", "", ""};
                m.overlay_option_count = 2;
                break;
            case Overlay::RESIGN_CONFIRM:
                m.overlay_title = "RESIGN THIS GAME?";
                m.overlay_options = {"RESIGN", "CANCEL", "", "", ""};
                m.overlay_option_count = 2;
                break;
            case Overlay::NONE:
                break;
        }

        return m;
    }

    Overlay active_overlay() const {
        if (app_overlay_ != Overlay::NONE) return app_overlay_;
        if (session_.pending() == ChessSession::Pending::PROMOTION) return Overlay::PROMOTION;
        if (session_.pending() == ChessSession::Pending::CLAIM_INTENDED) return Overlay::CLAIM_INTENDED;
        return Overlay::NONE;
    }

    void quarantine_bad_save() {
        std::string bad = std::string(save_path_) + ".bad";
        (void)unlink(bad.c_str());
        (void)rename(save_path_, bad.c_str());
    }

    void save_now() {
        const SaveData data = session_.save_data();
        const std::string bytes = encode_save(data);
        if (kb_save_atomic(save_path_, bytes.data(), bytes.size()) != 0) {
            runtime_message_ = "SAVE FAILED";
        } else if (runtime_message_ == "SAVE FAILED") {
            runtime_message_.clear();
        }
    }

    void arm_engine_guard(unsigned delay_ms) {
        (void)kb_timer_start(kb_, TIMER_ENGINE_GUARD, delay_ms, 0);
        engine_guard_armed_ = true;
    }

    void cancel_engine_guard() {
        if (engine_guard_armed_) {
            (void)kb_timer_cancel(kb_, TIMER_ENGINE_GUARD);
            engine_guard_armed_ = false;
        }
    }

    void schedule_settle(std::uint64_t mask) {
        settle_mask_ |= mask;
        (void)kb_timer_start(kb_, TIMER_SETTLE, SETTLE_MS, 0);
    }

    void present_interaction(std::uint64_t dirty) {
        if (dirty == 0) {
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            return;
        }
        schedule_settle(dirty);
        renderer_.draw_interaction(model(), dirty, KB_REFRESH_UI);
    }

    void present_overlay() {
        settle_mask_ = 0;
        (void)kb_timer_cancel(kb_, TIMER_SETTLE);
        renderer_.draw_full(model(), KB_REFRESH_CLEAN);
    }

    void stop_engine() {
        cancel_engine_guard();
        if (engine_watch_active_) {
            (void)kb_unwatch_fd(kb_, ENGINE_FD_ID);
            engine_watch_active_ = false;
        }
        engine_.shutdown();
        engine_synced_ = false;
        need_engine_newgame_ = false;
    }

    bool start_engine() {
        stop_engine();
        if (engine_path_.empty() || access(engine_path_.c_str(), X_OK) != 0) {
            runtime_message_ = "STOCKFISH MISSING";
            return false;
        }

        if (!engine_.start(engine_path_, session_.elo())) {
            runtime_message_ = "ENGINE START FAILED";
            return false;
        }

        if (kb_watch_fd(kb_, ENGINE_FD_ID, engine_.read_fd()) != 0) {
            engine_.shutdown();
            runtime_message_ = "ENGINE WATCH FAILED";
            return false;
        }

        engine_watch_active_ = true;
        engine_synced_ = false;
        need_engine_newgame_ = true;
        runtime_message_.clear();
        arm_engine_guard(ENGINE_HANDSHAKE_TIMEOUT_MS);
        return true;
    }

    void fail_engine(const char* reason, bool allow_restart) {
        cancel_engine_guard();
        if (engine_watch_active_) {
            (void)kb_unwatch_fd(kb_, ENGINE_FD_ID);
            engine_watch_active_ = false;
        }
        engine_.mark_pipe_failure(reason);
        engine_.shutdown();
        engine_synced_ = false;
        need_engine_newgame_ = false;
        runtime_message_ = "ENGINE FAILED";

        if (pending_undo_) {
            pending_undo_ = false;
            const std::uint64_t dirty = session_.undo();
            save_now();
            present_interaction(dirty);
        }

        if (allow_restart && !suspended_ &&
            session_.mode() != PlayMode::LOCAL_TWO_PLAYER && !session_.ended() &&
            engine_restart_attempts_ < 1) {
            ++engine_restart_attempts_;
            (void)start_engine();
        }
    }

    void service_engine() {
        if (suspended_ || pending_undo_ || session_.ended() ||
            session_.mode() == PlayMode::LOCAL_TWO_PLAYER || !engine_.alive()) {
            return;
        }

        if (engine_.state() == UciEngine::State::IDLE) {
            cancel_engine_guard();
        }

        if (engine_.new_game_ready()) {
            engine_.clear_new_game_ready();
            engine_synced_ = true;
            engine_restart_attempts_ = 0;
            cancel_engine_guard();
        }

        if (need_engine_newgame_ && engine_.state() == UciEngine::State::IDLE) {
            need_engine_newgame_ = false;
            engine_synced_ = false;
            if (!engine_.new_game()) {
                fail_engine("new game sync", true);
                return;
            }
            arm_engine_guard(ENGINE_SYNC_TIMEOUT_MS);
            return;
        }

        if (!engine_synced_ || engine_.state() != UciEngine::State::IDLE) return;

        if (session_.engine_should_claim()) {
            if (session_.claim_engine_current_draw()) {
                save_now();
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            }
            return;
        }

        if (!session_.engine_turn()) return;

        const unsigned think = think_time_ms(session_.elo());
        if (!engine_.search(session_.uci_history(), think)) {
            fail_engine("search start", true);
            return;
        }
        arm_engine_guard(think + 5000U);
        renderer_.draw_interaction(model(), 0, KB_REFRESH_UI);
    }

    void handle_engine_fd(unsigned flags) {
        if (!engine_watch_active_) return;

        bool ok = true;
        if (flags & KB_FD_READABLE) ok = engine_.drain();

        if (!ok) {
            fail_engine("stdout closed", true);
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            return;
        }

        if (flags & (KB_FD_ERROR | KB_FD_INVALID)) {
            fail_engine("pipe error", true);
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            return;
        }

        // HUP can arrive with final readable bytes. If the engine is still
        // logically alive after draining, it is nevertheless unusable.
        if ((flags & KB_FD_HANGUP) && engine_.alive()) {
            fail_engine("pipe hangup", true);
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            return;
        }

        if (engine_.state() == UciEngine::State::IDLE) cancel_engine_guard();

        const auto bestmove = engine_.take_bestmove();
        if (bestmove) {
            cancel_engine_guard();

            if (!engine_synced_ || !session_.engine_turn()) {
                fail_engine("unexpected bestmove", true);
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                return;
            }

            if (*bestmove == "(none)" || *bestmove == "0000") {
                fail_engine("empty bestmove in live game", true);
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                return;
            }

            if (session_.engine_can_claim_with_move(*bestmove)) {
                (void)session_.claim_engine_intended_draw(*bestmove);
                save_now();
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                return;
            }

            std::uint64_t dirty = 0;
            if (!session_.apply_engine_move(*bestmove, &dirty)) {
                fail_engine("illegal bestmove", true);
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                return;
            }

            engine_restart_attempts_ = 0;
            save_now();
            present_interaction(dirty);
        }

        if (pending_undo_ && engine_.state() == UciEngine::State::IDLE) {
            pending_undo_ = false;
            cancel_engine_guard();
            const std::uint64_t dirty = session_.undo();
            save_now();
            present_interaction(dirty);
        }

        service_engine();
    }

    void handle_timer(int id) {
        if (id == TIMER_SETTLE) {
            if (settle_mask_ != 0) {
                const std::uint64_t mask = settle_mask_;
                settle_mask_ = 0;
                renderer_.draw_interaction(model(), mask, KB_REFRESH_GRAY);
            }
            return;
        }

        if (id == TIMER_ENGINE_GUARD) {
            engine_guard_armed_ = false;
            if (engine_.alive() && engine_.state() != UciEngine::State::IDLE) {
                fail_engine("timeout", true);
                renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            }
        }
    }

    void handle_suspend() {
        suspended_ = true;
        save_now();
        settle_mask_ = 0;
        (void)kb_timer_cancel(kb_, TIMER_SETTLE);

        if (engine_.state() == UciEngine::State::SEARCHING) {
            if (engine_.stop()) arm_engine_guard(ENGINE_STOP_TIMEOUT_MS);
        }
    }

    void handle_resume() {
        suspended_ = false;
        renderer_.draw_full(model(), KB_REFRESH_CLEAN);
        service_engine();
    }

    void start_new_game(PlayMode mode, int elo) {
        app_overlay_ = Overlay::NONE;
        pending_new_mode_ = mode;
        pending_undo_ = false;
        settle_mask_ = 0;
        (void)kb_timer_cancel(kb_, TIMER_SETTLE);

        session_.reset(mode, elo);
        save_now();

        if (mode == PlayMode::LOCAL_TWO_PLAYER) {
            stop_engine();
            runtime_message_.clear();
        } else {
            engine_restart_attempts_ = 0;
            (void)start_engine();
        }

        renderer_.draw_full(model(), KB_REFRESH_CLEAN);
        service_engine();
    }

    void request_undo() {
        if (!session_.can_undo() || pending_undo_) return;

        if (engine_.state() == UciEngine::State::SEARCHING) {
            if (!engine_.stop()) {
                fail_engine("stop for undo", true);
                return;
            }
            pending_undo_ = true;
            arm_engine_guard(ENGINE_STOP_TIMEOUT_MS);
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
            return;
        }

        const std::uint64_t dirty = session_.undo();
        save_now();
        present_interaction(dirty);
        service_engine();
    }

    void handle_overlay_option(int option) {
        const Overlay overlay = active_overlay();

        if (overlay == Overlay::NEW_MODE) {
            if (option == 0 || option == 1) {
                pending_new_mode_ = option == 0 ? PlayMode::HUMAN_WHITE : PlayMode::HUMAN_BLACK;
                app_overlay_ = Overlay::NEW_LEVEL;
                present_overlay();
            } else if (option == 2) {
                start_new_game(PlayMode::LOCAL_TWO_PLAYER, session_.elo());
            }
            return;
        }

        if (overlay == Overlay::NEW_LEVEL) {
            static constexpr std::array<int, 5> levels = {1320, 1600, 2000, 2500, 3190};
            if (option >= 0 && option < static_cast<int>(levels.size())) {
                start_new_game(pending_new_mode_, levels[static_cast<std::size_t>(option)]);
            }
            return;
        }

        if (overlay == Overlay::PROMOTION) {
            static constexpr std::array<chess::PieceType::underlying, 4> pieces = {
                chess::PieceType::QUEEN, chess::PieceType::ROOK,
                chess::PieceType::BISHOP, chess::PieceType::KNIGHT};
            if (option >= 0 && option < static_cast<int>(pieces.size())) {
                const auto result = session_.choose_promotion(chess::PieceType(pieces[static_cast<std::size_t>(option)]));
                if (session_.pending() == ChessSession::Pending::CLAIM_INTENDED) {
                    present_overlay();
                } else if (result.moved) {
                    save_now();
                    renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                    service_engine();
                }
            }
            return;
        }

        if (overlay == Overlay::CLAIM_INTENDED) {
            if (option == 0) {
                if (session_.claim_pending_draw()) {
                    save_now();
                    renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                }
            } else if (option == 1) {
                const auto result = session_.play_pending_claim_move();
                if (result.moved) {
                    save_now();
                    renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                    service_engine();
                }
            }
            return;
        }

        if (overlay == Overlay::RESIGN_CONFIRM) {
            app_overlay_ = Overlay::NONE;
            if (option == 0) {
                session_.resign();
                save_now();
            }
            renderer_.draw_full(model(), KB_REFRESH_CLEAN);
        }
    }

    void handle_tap(int x, int y) {
        const UiModel current = model();

        if (current.overlay != Overlay::NONE) {
            const int option = renderer_.overlay_option_at(current, x, y);
            if (option >= 0) handle_overlay_option(option);
            return;
        }

        const UiButton button = renderer_.button_at(x, y);
        switch (button) {
            case UiButton::NEW_GAME:
                app_overlay_ = Overlay::NEW_MODE;
                present_overlay();
                return;
            case UiButton::UNDO:
                request_undo();
                return;
            case UiButton::CLAIM_DRAW:
                if (session_.claim_current_draw()) {
                    save_now();
                    renderer_.draw_full(model(), KB_REFRESH_CLEAN);
                }
                return;
            case UiButton::RESIGN:
                if (!session_.ended() && session_.human_turn()) {
                    app_overlay_ = Overlay::RESIGN_CONFIRM;
                    present_overlay();
                }
                return;
            case UiButton::EXIT:
                running_ = false;
                return;
            case UiButton::NONE:
                break;
        }

        const int square = renderer_.square_at(current, x, y);
        if (square < 0) return;

        const auto result = session_.tap_square(square);
        if (!result.changed) return;

        if (session_.pending() != ChessSession::Pending::NONE) {
            present_overlay();
            return;
        }

        if (result.moved) save_now();
        present_interaction(result.dirty_squares);
        if (result.moved) service_engine();
    }

    KBGame* kb_ = nullptr;
    Renderer renderer_;
    ChessSession session_;
    UciEngine engine_;

    std::string engine_path_;
    char save_path_[512]{};
    std::string runtime_message_;

    bool running_ = true;
    bool suspended_ = false;
    bool engine_watch_active_ = false;
    bool engine_synced_ = false;
    bool need_engine_newgame_ = false;
    bool engine_guard_armed_ = false;
    bool pending_undo_ = false;
    int engine_restart_attempts_ = 0;

    std::uint64_t settle_mask_ = 0;

    Overlay app_overlay_ = Overlay::NONE;
    PlayMode pending_new_mode_ = PlayMode::HUMAN_WHITE;
};

}  // namespace inkchess

int main(int argc, char** argv) {
    KBConfig cfg;
    kb_config_defaults(&cfg);
    cfg.app_id = "chess";
    cfg.title = "InkChess";
    cfg.partial_refresh_limit = 22;
    cfg.clean_interval_ms = 60000;
    cfg.accumulated_coverage_x100 = 220;

    KBGame* kb = kb_create(&cfg);
    if (!kb) {
        std::fprintf(stderr, "inkchess: KBGE initialization failed\n");
        return 1;
    }

    const std::string engine_path = argc >= 2 ? argv[1] : "";
    inkchess::App app(kb, engine_path);
    if (!app.initialize()) {
        std::fprintf(stderr, "inkchess: initialization failed: %s\n", kb_last_error(kb));
        kb_destroy(kb);
        return 1;
    }

    while (app.running()) {
        KBEvent ev;
        const int rc = kb_poll_event(kb, &ev, -1);
        if (rc < 0) {
            std::fprintf(stderr, "inkchess: event error: %s\n", kb_last_error(kb));
            break;
        }
        if (rc == 0) continue;
        app.on_event(ev);
    }

    app.shutdown();
    kb_destroy(kb);
    return 0;
}
