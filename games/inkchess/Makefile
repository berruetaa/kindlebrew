CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR := build/host
TEST_BIN := $(BUILD_DIR)/test_rules

.PHONY: all test clean

all: $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(TEST_BIN): tests/test_rules.cpp src/game_rules.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_rules.cpp -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

clean:
	rm -rf build
