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
RENDERER_TEST := $(HOST_BUILD)/test_renderer
HOST_APP := $(HOST_BUILD)/inkchess

APP_SRC := src/main.cpp src/renderer.cpp src/chess_session.cpp src/save_game.cpp src/uci_engine.cpp
GNOME_ASSET_SRC := assets/gnome-simple
ASSET_TOOL := tools/rasterize_pieces.py
ASSET_NAMES := whitePawn whiteKnight whiteBishop whiteRook whiteQueen whiteKing \
               blackPawn blackKnight blackBishop blackRook blackQueen blackKing
ASSET_FILES := $(addprefix $(ASSET_BUILD)/,$(addsuffix .r8a8,$(ASSET_NAMES)))
ASSET_STAMP := $(ASSET_BUILD)/.stamp

.PHONY: all test clean kindle assets

all: $(RULES_TEST) $(UCI_TEST) $(FAKE_ENGINE) $(SAVE_TEST) $(SESSION_TEST) $(RENDERER_TEST) $(HOST_APP)

$(HOST_BUILD):
	mkdir -p $@

$(KINDLE_BUILD):
	mkdir -p $@

$(ASSET_BUILD):
	mkdir -p $@

$(RULES_TEST): tests/test_rules.cpp src/game_rules.hpp third_party/chess.hpp | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/test_rules.cpp -o $@

$(FAKE_ENGINE): tests/fake_uci_engine.cpp | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/fake_uci_engine.cpp -o $@

$(UCI_TEST): tests/test_uci.cpp src/uci_engine.cpp src/uci_engine.hpp | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/test_uci.cpp src/uci_engine.cpp -pthread -o $@

$(SAVE_TEST): tests/test_save.cpp src/save_game.cpp src/save_game.hpp third_party/chess.hpp | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/test_save.cpp src/save_game.cpp -o $@

$(SESSION_TEST): tests/test_session.cpp src/chess_session.cpp src/chess_session.hpp src/game_rules.hpp src/save_game.hpp third_party/chess.hpp | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/test_session.cpp src/chess_session.cpp -o $@

$(RENDERER_TEST): tests/test_renderer.cpp src/renderer.cpp src/renderer.hpp src/save_game.hpp third_party/chess.hpp $(ENGINE_DIR)/build/host/libkbgame.a | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) -DINKCHESS_TEST_ASSETS $(CXXFLAGS) $(COMMON_CXXFLAGS) tests/test_renderer.cpp src/renderer.cpp $(ENGINE_DIR)/build/host/libkbgame.a -o $@

$(ENGINE_DIR)/build/host/libkbgame.a:
	$(MAKE) -C $(ENGINE_DIR) all

$(HOST_APP): $(APP_SRC) src/renderer.hpp src/chess_session.hpp src/save_game.hpp src/uci_engine.hpp third_party/chess.hpp $(ENGINE_DIR)/build/host/libkbgame.a | $(HOST_BUILD)
	$(CXX) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CXXFLAGS) $(COMMON_CXXFLAGS) $(APP_SRC) $(ENGINE_DIR)/build/host/libkbgame.a -pthread -o $@

test: all
	$(RULES_TEST)
	$(UCI_TEST) $(FAKE_ENGINE)
	$(SAVE_TEST)
	$(SESSION_TEST)
	$(RENDERER_TEST)

$(ASSET_STAMP): $(ASSET_TOOL) $(addprefix $(GNOME_ASSET_SRC)/,$(addsuffix .svg,$(ASSET_NAMES))) | $(ASSET_BUILD)
	python3 $(ASSET_TOOL) --src $(GNOME_ASSET_SRC) --out $(ASSET_BUILD)
	@for f in $(ASSET_FILES); do test -s "$$f"; test "$$(wc -c < "$$f")" -eq 32768; done
	touch $@

assets: $(ASSET_STAMP)

# Kindle cross build ----------------------------------------------------------
KINDLE_PREFIX ?= arm-kindlehf-linux-gnueabihf-
KCXX := $(KINDLE_PREFIX)g++
KSTRIP := $(KINDLE_PREFIX)strip
FBINK_DIR ?= $(ENGINE_DIR)/vendor/FBInk
KINDLE_CPPFLAGS := $(COMMON_CPPFLAGS) -DKB_KINDLE -DFBINK_FOR_KINDLE -DFBINK_MINIMAL -DFBINK_WITH_DRAW -DFBINK_WITH_INPUT -I$(FBINK_DIR)
KINDLE_CXXFLAGS := -Os $(COMMON_CXXFLAGS) -ffunction-sections -fdata-sections
KINDLE_LDFLAGS := -Wl,--gc-sections -static-libstdc++ -static-libgcc

$(KINDLE_BUILD)/inkchess: $(APP_SRC) src/renderer.hpp src/chess_session.hpp src/save_game.hpp src/uci_engine.hpp third_party/chess.hpp $(ENGINE_DIR)/build/kindle/libkbgame.a $(ASSET_STAMP) | $(KINDLE_BUILD)
	@test -f "$(FBINK_DIR)/Release/libfbink.a" || (echo "Build FBInk staticlib first"; exit 1)
	$(KCXX) $(KINDLE_CPPFLAGS) $(KINDLE_CXXFLAGS) $(APP_SRC) $(ENGINE_DIR)/build/kindle/libkbgame.a $(FBINK_DIR)/Release/libfbink.a -lm -pthread $(KINDLE_LDFLAGS) -o $@
	$(KSTRIP) --strip-unneeded $@

kindle: $(KINDLE_BUILD)/inkchess
	file $(KINDLE_BUILD)/inkchess
	@for f in $(ASSET_FILES); do test -s "$$f"; done

clean:
	rm -rf build
