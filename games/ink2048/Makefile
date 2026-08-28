CC ?= cc
CFLAGS ?= -O2

ENGINE_DIR ?= ../../engine
BUILD := build
HOST_BUILD := $(BUILD)/host
KINDLE_BUILD := $(BUILD)/kindle

COMMON_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
COMMON_CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Isrc -I$(ENGINE_DIR)/include

.PHONY: all test clean kindle

$(HOST_BUILD):
	mkdir -p $@

$(KINDLE_BUILD):
	mkdir -p $@

$(HOST_BUILD)/test_game2048: tests/test_game2048.c src/game2048.c src/game2048.h | $(HOST_BUILD)
	$(CC) $(CFLAGS) $(COMMON_CFLAGS) -Isrc tests/test_game2048.c src/game2048.c -o $@

test: $(HOST_BUILD)/test_game2048
	$(HOST_BUILD)/test_game2048

all: test

KINDLE_PREFIX ?= arm-kindlehf-linux-gnueabihf-
KCC := $(KINDLE_PREFIX)gcc
KSTRIP := $(KINDLE_PREFIX)strip
FBINK_DIR ?= $(ENGINE_DIR)/vendor/FBInk
KINDLE_CFLAGS := -Os $(COMMON_CFLAGS) -Werror -ffunction-sections -fdata-sections
KINDLE_CPPFLAGS := $(COMMON_CPPFLAGS) -DKB_KINDLE -DFBINK_FOR_KINDLE -DFBINK_MINIMAL -DFBINK_WITH_DRAW -DFBINK_WITH_INPUT -I$(FBINK_DIR)
KINDLE_LDFLAGS := -Wl,--gc-sections

$(KINDLE_BUILD)/ink2048: src/main.c src/game2048.c src/game2048.h $(ENGINE_DIR)/build/kindle/libkbgame.a | $(KINDLE_BUILD)
	@test -f "$(FBINK_DIR)/Release/libfbink.a" || (echo "Build FBInk staticlib first"; exit 1)
	$(KCC) $(KINDLE_CPPFLAGS) $(KINDLE_CFLAGS) src/main.c src/game2048.c 		$(ENGINE_DIR)/build/kindle/libkbgame.a $(FBINK_DIR)/Release/libfbink.a -lm 		$(KINDLE_LDFLAGS) -o $@
	$(KSTRIP) --strip-unneeded $@

kindle: $(KINDLE_BUILD)/ink2048
	file $(KINDLE_BUILD)/ink2048

clean:
	rm -rf $(BUILD)
