CXX ?= g++
# InkChess host and kindlehf build
CPPFLAGS ?=
CXXFLAGS ?= -O2

ENGINE_DIR ?= ../../engine
BUILD_DIR := build
HOST_BUILD := $(BUILD_DIR)/host
KINDLE_BUILD := $(BUILD_DIR)/kindle
ASSET_BUILD := $(BUILD_DIR)/assets

COMMON_CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Isrc -I$(ENGINE_DIR)/include
COMMON_CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror

RULES_TEST := $(HOST_BUILD)/test_rules
UCI_TEST := $(HOST_BUILD)/test_uci
FAKE_ENGINE := $(HOST_BUILD)/fake_uci_engine
SAVE_TEST := $(HOST_BUILD)/test_save
SESSION_TEST := $(HOST_BUILD)/test_session
HOST_APP := $(HOST_BUILD)/inkchess

APP_SRC := src/main.cpp src/renderer.cpp src/chess_session.cpp src/save_game.cpp src/uci_engine.cpp
GNOME_ASSET_SRC := assets/gnome-simple
ASSET_TOOL := tools/rasterize_pieces.py
ASSET_NAMES := whitePawn whiteKnight whiteBishop whiteRook whiteQueen whiteKing \
               blackPawn blackKnight blackBishop blackRook blackQueen blackKing
ASSET_FILES := $(addprefix $(ASSET_BUILD)/,$(addsuffix .r8a8,$(ASSET_NAMES)))
ASSET_STAMP := $(ASSET_BUILD)/.stamp

.PHONY: all test clean kindle assets

all: $(RULES_TEST) $(UCI_TEST) $(FAKE_ENGINE) $(SAVE_TEST) $(SESSION_TEST) $(HOST_APP)

$(HOST_BUILD):
	mkdir -p $@

$(KINDLE_BUILD):
	mkdir -p $@

$(ASSET_BUILD):
	mkdir -p $@

$(RULES_TEST): tests/test_rules.cpp src/game_rules.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_rules.cpp -o $@

$(FAKE_ENGINE): tests/fake_uci_engine.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/fake_uci_engine.cpp -o $@

$(UCI_TEST): tests/test_uci.cpp src/uci_engine.cpp src/uci_engine.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_uci.cpp src/uci_engine.cpp -pthread -o $@

$(SAVE_TEST): tests/test_save.cpp src/save_game.cpp src/save_game.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_save.cpp src/save_game.cpp -o $@

$(SESSION_TEST): tests/test_session.cpp src/chess_session.cpp src/chess_session.hpp src/game_rules.hpp src/save_game.hpp third_party/chess.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_session.cpp src/chess_session.cpp -o $@

test: all
	$(RULES_TEST)
	$(UCI_TEST) $(FAKE_ENGINE)
	$(SAVE_TEST)
	$(SESSION_TEST)

clean:
	rm -rf build
