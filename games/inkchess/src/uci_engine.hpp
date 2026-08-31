#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>

namespace inkchess {

class UciEngine {
   public:
    enum class State {
        DEAD = 0,
        WAIT_UCIOK,
        WAIT_READY_STARTUP,
        IDLE,
        SEARCHING,
        STOPPING,
        WAIT_READY_NEWGAME
    };

    UciEngine() = default;
    ~UciEngine();

    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    bool start(const std::string& executable, int elo);
    void shutdown();

    [[nodiscard]] int read_fd() const noexcept { return stdout_fd_; }
    [[nodiscard]] pid_t pid() const noexcept { return pid_; }
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool alive() const noexcept { return pid_ > 0 && state_ != State::DEAD; }
    [[nodiscard]] bool ready() const noexcept { return state_ == State::IDLE; }
    [[nodiscard]] const std::string& last_error() const noexcept { return error_; }

    // Drain currently available stdout. Call when KB_EVENT_FD reports readable.
    // Returns false on EOF or an unrecoverable read/protocol error.
    bool drain();

    // HUP/ERR/NVAL from the outer poll loop.
    void mark_pipe_failure(std::string_view why);

    // A new game is asynchronous. If a search is active it is stopped first and
    // the stale bestmove is consumed before ucinewgame/isready are issued.
    bool new_game();

    // Search is accepted only while fully synchronized/idle.
    bool search(const std::vector<std::string>& moves, unsigned movetime_ms);

    // Stop an active search and discard its result.
    bool stop();

    // Returns one validated-format UCI bestmove token produced by the current
    // active search. Legality against the board is intentionally the GUI's job.
    std::optional<std::string> take_bestmove();

    // True when a stop requested for new_game has completed and synchronization
    // subsequently reached IDLE.
    [[nodiscard]] bool new_game_ready() const noexcept { return new_game_ready_; }
    void clear_new_game_ready() noexcept { new_game_ready_ = false; }

   private:
    enum class AfterStop { NONE = 0, NEW_GAME };

    bool send_line(std::string_view line);
    bool handle_line(std::string_view line);
    bool configure_after_uciok();
    bool begin_new_game_sync();
    void fail(std::string message);
    void close_fds() noexcept;
    void reap_nonblocking() noexcept;

    pid_t pid_ = -1;
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    State state_ = State::DEAD;
    AfterStop after_stop_ = AfterStop::NONE;

    std::string read_buffer_;
    std::string error_;
    std::optional<std::string> bestmove_;
    int elo_ = 1600;
    bool new_game_ready_ = false;
};

}  // namespace inkchess
