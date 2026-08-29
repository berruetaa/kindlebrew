#include <cassert>
#include <chrono>
#include <poll.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "../src/uci_engine.hpp"

using inkchess::UciEngine;

static void pump_until(UciEngine& engine, UciEngine::State wanted, int timeout_ms = 1000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (engine.state() != wanted && std::chrono::steady_clock::now() < end) {
        struct pollfd pfd { engine.read_fd(), POLLIN | POLLPRI, 0 };
        const int rc = poll(&pfd, 1, 50);
        assert(rc >= 0);
        if (rc > 0) {
            if (pfd.revents & (POLLIN | POLLPRI)) (void)engine.drain();
            if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                if (!(pfd.revents & (POLLIN | POLLPRI))) {
                    engine.mark_pipe_failure("test poll failure");
                }
            }
        }
    }
    assert(engine.state() == wanted);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string fake = argv[1];

    UciEngine engine;
    assert(engine.start(fake, 1600));
    pump_until(engine, UciEngine::State::IDLE);
    assert(engine.ready());

    // A normal result becomes visible only after a complete bestmove line.
    assert(engine.search({"e2e4"}, 101));
    pump_until(engine, UciEngine::State::IDLE);
    auto normal = engine.take_bestmove();
    assert(normal.has_value() && *normal == "e7e5");

    // Search command with a held result exposes nothing before cancellation.
    assert(engine.search({"e2e4"}, 100));
    assert(engine.state() == UciEngine::State::SEARCHING);
    assert(!engine.take_bestmove().has_value());

    // new_game while searching must stop, consume stale bestmove, then wait for
    // a post-ucinewgame readyok before becoming idle.
    assert(engine.new_game());
    assert(engine.state() == UciEngine::State::STOPPING);
    pump_until(engine, UciEngine::State::IDLE);
    assert(engine.new_game_ready());
    engine.clear_new_game_ready();
    assert(!engine.take_bestmove().has_value());

    // A fresh search can be stopped without leaking its stale result.
    assert(engine.search({"e2e4"}, 100));
    assert(engine.stop());
    assert(engine.state() == UciEngine::State::STOPPING);
    pump_until(engine, UciEngine::State::IDLE);
    assert(!engine.take_bestmove().has_value());

    // Invalid history is rejected before it reaches the child.
    assert(!engine.search({"not-a-move"}, 100));
    assert(engine.state() == UciEngine::State::IDLE);

    engine.shutdown();
    assert(engine.state() == UciEngine::State::DEAD);
    return 0;
}
