#include <iostream>
#include <string>

int main() {
    std::string line;
    bool searching = false;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name FakeFish\n";
            std::cout << "option name Threads type spin default 1 min 1 max 1\n";
            std::cout << "option name Hash type spin default 16 min 1 max 64\n";
            std::cout << "option name Ponder type check default false\n";
            std::cout << "option name UCI_LimitStrength type check default false\n";
            std::cout << "option name UCI_Elo type spin default 1600 min 1320 max 3190\n";
            std::cout << "uciok\n" << std::flush;
        } else if (line == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (line.rfind("go ", 0) == 0) {
            searching = true;
            // Deliberately wait for an explicit "emit" test command so the
            // harness can exercise stop/new-game races deterministically.
        } else if (line == "emit") {
            if (searching) {
                std::cout << "info depth 1 nodes 20\n";
                std::cout << "bestmove e7e5\n" << std::flush;
                searching = false;
            }
        } else if (line == "stop") {
            if (searching) {
                std::cout << "bestmove e7e5\n" << std::flush;
                searching = false;
            }
        } else if (line == "quit") {
            return 0;
        }
    }
    return 0;
}
