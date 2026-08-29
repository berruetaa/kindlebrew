#include "uci_engine.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace inkchess {

namespace {

constexpr std::size_t kMaxBufferedOutput = 64U * 1024U;

bool valid_bestmove_token(std::string_view move) {
    if (move == "(none)" || move == "0000") return true;
    if (move.size() != 4 && move.size() != 5) return false;
    const auto file = [](char c) { return c >= 'a' && c <= 'h'; };
    const auto rank = [](char c) { return c >= '1' && c <= '8'; };
    if (!file(move[0]) || !rank(move[1]) || !file(move[2]) || !rank(move[3])) return false;
    if (move.size() == 5) {
        const char p = move[4];
        if (p != 'q' && p != 'r' && p != 'b' && p != 'n') return false;
    }
    return true;
}

}  // namespace

UciEngine::~UciEngine() {
    shutdown();
}

void UciEngine::close_fds() noexcept {
    if (stdin_fd_ >= 0) {
        close(stdin_fd_);
        stdin_fd_ = -1;
    }
    if (stdout_fd_ >= 0) {
        close(stdout_fd_);
        stdout_fd_ = -1;
    }
}

void UciEngine::reap_nonblocking() noexcept {
    if (pid_ <= 0) return;
    int status = 0;
    const pid_t rc = waitpid(pid_, &status, WNOHANG);
    if (rc == pid_) pid_ = -1;
}

void UciEngine::fail(std::string message) {
    error_ = std::move(message);
    state_ = State::DEAD;
    after_stop_ = AfterStop::NONE;
    bestmove_.reset();
    new_game_ready_ = false;
    close_fds();
    reap_nonblocking();
}

bool UciEngine::start(const std::string& executable, int elo) {
    shutdown();
    error_.clear();
    bestmove_.reset();
    read_buffer_.clear();
    new_game_ready_ = false;
    after_stop_ = AfterStop::NONE;
    elo_ = std::clamp(elo, 1320, 3190);

    int to_child[2] = {-1, -1};
    int from_child[2] = {-1, -1};
    if (pipe(to_child) != 0 || pipe(from_child) != 0) {
        const std::string why = std::strerror(errno);
        if (to_child[0] >= 0) close(to_child[0]);
        if (to_child[1] >= 0) close(to_child[1]);
        if (from_child[0] >= 0) close(from_child[0]);
        if (from_child[1] >= 0) close(from_child[1]);
        fail("pipe: " + why);
        return false;
    }

    const pid_t child = fork();
    if (child < 0) {
        const std::string why = std::strerror(errno);
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        fail("fork: " + why);
        return false;
    }

    if (child == 0) {
        // Child owns stdin/stdout. Keep diagnostics from filling an unattended
        // pipe: stderr goes to /dev/null.
        int devnull = open("/dev/null", O_WRONLY);
        if (dup2(to_child[0], STDIN_FILENO) < 0 ||
            dup2(from_child[1], STDOUT_FILENO) < 0 ||
            (devnull >= 0 && dup2(devnull, STDERR_FILENO) < 0)) {
            _exit(126);
        }

        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        if (devnull >= 0 && devnull != STDERR_FILENO) close(devnull);

        execl(executable.c_str(), executable.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);
    pid_ = child;
    stdin_fd_ = to_child[1];
    stdout_fd_ = from_child[0];

    const int flags = fcntl(stdout_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(stdout_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        const std::string why = std::strerror(errno);
        shutdown();
        error_ = "fcntl(O_NONBLOCK): " + why;
        return false;
    }

    state_ = State::WAIT_UCIOK;
    if (!send_line("uci")) {
        shutdown();
        return false;
    }
    return true;
}

bool UciEngine::send_line(std::string_view line) {
    if (stdin_fd_ < 0 || state_ == State::DEAD) return false;

    std::string command(line);
    command.push_back('\n');
    const char* p = command.data();
    std::size_t left = command.size();

    while (left > 0) {
        const ssize_t n = write(stdin_fd_, p, left);
        if (n > 0) {
            p += n;
            left -= static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        fail("write Stockfish: " + std::string(std::strerror(errno)));
        return false;
    }
    return true;
}

bool UciEngine::configure_after_uciok() {
    if (!send_line("setoption name Threads value 1")) return false;
    if (!send_line("setoption name Hash value 16")) return false;
    if (!send_line("setoption name Ponder value false")) return false;
    if (!send_line("setoption name UCI_LimitStrength value true")) return false;
    if (!send_line("setoption name UCI_Elo value " + std::to_string(elo_))) return false;
    if (!send_line("isready")) return false;
    state_ = State::WAIT_READY_STARTUP;
    return true;
}

bool UciEngine::begin_new_game_sync() {
    if (!send_line("ucinewgame")) return false;
    if (!send_line("isready")) return false;
    state_ = State::WAIT_READY_NEWGAME;
    return true;
}

bool UciEngine::handle_line(std::string_view line) {
    while (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) return true;

    if (line == "uciok") {
        if (state_ != State::WAIT_UCIOK) {
            // Duplicate uciok is harmless noise, but never changes an active
            // search generation.
            return true;
        }
        return configure_after_uciok();
    }

    if (line == "readyok") {
        if (state_ == State::WAIT_READY_STARTUP) {
            state_ = State::IDLE;
            return true;
        }
        if (state_ == State::WAIT_READY_NEWGAME) {
            state_ = State::IDLE;
            new_game_ready_ = true;
            return true;
        }
        return true;
    }

    constexpr std::string_view prefix = "bestmove ";
    if (line.rfind(prefix, 0) == 0) {
        std::string_view rest = line.substr(prefix.size());
        const std::size_t space = rest.find(' ');
        const std::string_view token = rest.substr(0, space);

        if (!valid_bestmove_token(token)) {
            fail("malformed Stockfish bestmove");
            return false;
        }

        if (state_ == State::SEARCHING) {
            bestmove_ = std::string(token);
            state_ = State::IDLE;
            return true;
        }

        if (state_ == State::STOPPING) {
            const AfterStop action = after_stop_;
            after_stop_ = AfterStop::NONE;
            state_ = State::IDLE;
            if (action == AfterStop::NEW_GAME) return begin_new_game_sync();
            return true;
        }

        // A bestmove outside SEARCHING/STOPPING belongs to a stale generation.
        // Consume and ignore it; never expose it to the board.
        return true;
    }

    // id/option/info/copyprotection/registration/string output is informational.
    return true;
}

bool UciEngine::drain() {
    if (stdout_fd_ < 0 || state_ == State::DEAD) return false;

    char chunk[4096];
    for (;;) {
        const ssize_t n = read(stdout_fd_, chunk, sizeof(chunk));
        if (n > 0) {
            read_buffer_.append(chunk, static_cast<std::size_t>(n));
            if (read_buffer_.size() > kMaxBufferedOutput) {
                fail("Stockfish output line/buffer exceeded safety limit");
                return false;
            }

            for (;;) {
                const std::size_t nl = read_buffer_.find('\n');
                if (nl == std::string::npos) break;
                const std::string line = read_buffer_.substr(0, nl);
                read_buffer_.erase(0, nl + 1);
                if (!handle_line(line)) return false;
            }
            continue;
        }

        if (n == 0) {
            fail("Stockfish closed stdout");
            return false;
        }

        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;

        fail("read Stockfish: " + std::string(std::strerror(errno)));
        return false;
    }
}

void UciEngine::mark_pipe_failure(std::string_view why) {
    if (state_ == State::DEAD) return;
    fail("Stockfish pipe: " + std::string(why));
}

bool UciEngine::new_game() {
    new_game_ready_ = false;
    bestmove_.reset();

    if (state_ == State::IDLE) {
        return begin_new_game_sync();
    }

    if (state_ == State::SEARCHING) {
        if (!send_line("stop")) return false;
        after_stop_ = AfterStop::NEW_GAME;
        state_ = State::STOPPING;
        return true;
    }

    return false;
}

bool UciEngine::search(const std::vector<std::string>& moves, unsigned movetime_ms) {
    if (state_ != State::IDLE || movetime_ms == 0) return false;

    std::string position = "position startpos";
    if (!moves.empty()) {
        position += " moves";
        for (const auto& move : moves) {
            if (!valid_bestmove_token(move) || move == "(none)" || move == "0000") {
                error_ = "invalid UCI move in position history";
                return false;
            }
            position.push_back(' ');
            position += move;
        }
    }

    bestmove_.reset();
    if (!send_line(position)) return false;
    if (!send_line("go movetime " + std::to_string(movetime_ms))) return false;
    state_ = State::SEARCHING;
    return true;
}

bool UciEngine::stop() {
    if (state_ != State::SEARCHING) return state_ == State::IDLE;
    if (!send_line("stop")) return false;
    after_stop_ = AfterStop::NONE;
    state_ = State::STOPPING;
    bestmove_.reset();
    return true;
}

std::optional<std::string> UciEngine::take_bestmove() {
    auto result = std::move(bestmove_);
    bestmove_.reset();
    return result;
}

void UciEngine::shutdown() {
    if (pid_ <= 0) {
        close_fds();
        state_ = State::DEAD;
        return;
    }

    // Ask politely first. Do not wait for a stopped bestmove here: quit is
    // defined to terminate the UCI process and this is an exit path.
    if (stdin_fd_ >= 0) {
        if (state_ == State::SEARCHING) {
            (void)send_line("stop");
        }
        if (state_ != State::DEAD) {
            (void)send_line("quit");
        }
    }

    close_fds();

    const auto wait_for = [this](std::chrono::milliseconds budget) {
        const auto end = std::chrono::steady_clock::now() + budget;
        while (std::chrono::steady_clock::now() < end) {
            int status = 0;
            const pid_t rc = waitpid(pid_, &status, WNOHANG);
            if (rc == pid_ || (rc < 0 && errno == ECHILD)) {
                pid_ = -1;
                return true;
            }
            if (rc < 0 && errno != EINTR) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    };

    if (!wait_for(std::chrono::milliseconds(400)) && pid_ > 0) {
        (void)kill(pid_, SIGTERM);
        if (!wait_for(std::chrono::milliseconds(200)) && pid_ > 0) {
            (void)kill(pid_, SIGKILL);
            int status = 0;
            while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
            }
            pid_ = -1;
        }
    }

    state_ = State::DEAD;
    after_stop_ = AfterStop::NONE;
    bestmove_.reset();
    new_game_ready_ = false;
    read_buffer_.clear();
}

}  // namespace inkchess
