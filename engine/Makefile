CC ?= cc
AR ?= ar
CFLAGS ?= -O2
CPPFLAGS ?=

BUILD := build
HOST_BUILD := $(BUILD)/host
KINDLE_BUILD := $(BUILD)/kindle

COMMON_CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
COMMON_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes

CORE_SRC := src/kb_core.c src/kb_canvas.c
HOST_SRC := $(CORE_SRC) src/kb_backend_headless.c
HOST_OBJ := $(patsubst src/%.c,$(HOST_BUILD)/%.o,$(HOST_SRC))

.PHONY: all test clean kindle

all: $(HOST_BUILD)/libkbgame.a $(HOST_BUILD)/inklab

$(HOST_BUILD):
	mkdir -p $@

$(HOST_BUILD)/%.o: src/%.c | $(HOST_BUILD)
	$(CC) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CFLAGS) $(COMMON_CFLAGS) -c $< -o $@

$(HOST_BUILD)/libkbgame.a: $(HOST_OBJ)
	$(AR) rcs $@ $^

$(HOST_BUILD)/inklab: examples/inklab.c $(HOST_BUILD)/libkbgame.a
	$(CC) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CFLAGS) $(COMMON_CFLAGS) $< $(HOST_BUILD)/libkbgame.a -o $@

$(HOST_BUILD)/test_engine: tests/test_engine.c $(HOST_BUILD)/libkbgame.a
	$(CC) $(CPPFLAGS) $(COMMON_CPPFLAGS) $(CFLAGS) $(COMMON_CFLAGS) $< $(HOST_BUILD)/libkbgame.a -o $@

test: $(HOST_BUILD)/test_engine
	$(HOST_BUILD)/test_engine

# Kindle cross build ----------------------------------------------------------
KINDLE_PREFIX ?= arm-kindlehf-linux-gnueabihf-
KCC := $(KINDLE_PREFIX)gcc
KAR := $(KINDLE_PREFIX)ar
KSTRIP := $(KINDLE_PREFIX)strip
FBINK_DIR ?= ../FBInk
KINDLE_CPPFLAGS := $(COMMON_CPPFLAGS) -DKB_KINDLE -DFBINK_FOR_KINDLE -DFBINK_MINIMAL -DFBINK_WITH_DRAW -DFBINK_WITH_INPUT -I$(FBINK_DIR)
KINDLE_CFLAGS := -Os -std=c11 -Wall -Wextra -Wshadow -ffunction-sections -fdata-sections
KINDLE_LDFLAGS := -Wl,--gc-sections
KINDLE_SRC := $(CORE_SRC) src/kb_backend_headless.c src/kb_backend_kindle.c
KINDLE_OBJ := $(patsubst src/%.c,$(KINDLE_BUILD)/%.o,$(KINDLE_SRC))

$(KINDLE_BUILD):
	mkdir -p $@

$(KINDLE_BUILD)/%.o: src/%.c | $(KINDLE_BUILD)
	$(KCC) $(KINDLE_CPPFLAGS) $(KINDLE_CFLAGS) -c $< -o $@

$(KINDLE_BUILD)/libkbgame.a: $(KINDLE_OBJ)
	$(KAR) rcs $@ $^

$(KINDLE_BUILD)/inklab: examples/inklab.c $(KINDLE_BUILD)/libkbgame.a
	@test -f "$(FBINK_DIR)/Release/libfbink.a" || (echo "Build FBInk staticlib first"; exit 1)
	$(KCC) $(KINDLE_CPPFLAGS) $(KINDLE_CFLAGS) $< $(KINDLE_BUILD)/libkbgame.a \
		$(FBINK_DIR)/Release/libfbink.a -lm \
		$(KINDLE_LDFLAGS) -o $@
	$(KSTRIP) --strip-unneeded $@

kindle: $(KINDLE_BUILD)/inklab
	file $(KINDLE_BUILD)/inklab

clean:
	rm -rf $(BUILD)
