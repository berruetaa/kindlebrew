CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR := build/host
RULES_TEST := $(BUILD_DIR)/test_rules
UCI_TEST := $(BUILD_DIR)/test_uci
FAKE_ENGINE := $(BUILD_DIR)/fake_uci_engine
SAVE_TEST := $(BUILD_DIR)/test_save

.PHONY: all test clean

all: $(RULES_TEST) $(UCI_TEST) $(FAKE_ENGINE) $(SAVE_TEST)

$(BUILD_DIR):
	mkdir -p $@

$(RULES_TEST): tests/test_rules.cpp src/game_rules.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_rules.cpp -o $@

$(FAKE_ENGINE): tests/fake_uci_engine.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/fake_uci_engine.cpp -o $@

$(UCI_TEST): tests/test_uci.cpp src/uci_engine.cpp src/uci_engine.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_uci.cpp src/uci_engine.cpp -pthread -o $@

$(SAVE_TEST): tests/test_save.cpp src/save_game.cpp src/save_game.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_save.cpp src/save_game.cpp -o $@

test: all
	$(RULES_TEST)
	$(UCI_TEST) $(FAKE_ENGINE)
	$(SAVE_TEST)

clean:
	rm -rf build
