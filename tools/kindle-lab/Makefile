KINDLE_PREFIX ?= arm-kindlehf-linux-gnueabihf-
CC := $(KINDLE_PREFIX)gcc
STRIP := $(KINDLE_PREFIX)strip
FBINK_DIR ?= ../../engine/vendor/FBInk
BUILD := build
CFLAGS := -Os -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror
FBINK_FLAGS := -DFBINK_FOR_KINDLE -DFBINK_MINIMAL -DFBINK_WITH_DRAW -DFBINK_WITH_INPUT -I$(FBINK_DIR)

.PHONY: all clean

all: $(BUILD)/close-fds-exec $(BUILD)/uinput-touch \
     $(BUILD)/fbink-state $(BUILD)/framebuffer-dump

$(BUILD):
	mkdir -p $@

$(BUILD)/close-fds-exec: close-fds-exec.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
	$(STRIP) --strip-unneeded $@

$(BUILD)/uinput-touch: uinput-touch.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
	$(STRIP) --strip-unneeded $@

$(BUILD)/fbink-state: fbink-state.c $(FBINK_DIR)/Release/libfbink.a | $(BUILD)
	$(CC) $(CFLAGS) $(FBINK_FLAGS) $< $(FBINK_DIR)/Release/libfbink.a -lm -o $@
	$(STRIP) --strip-unneeded $@

$(BUILD)/framebuffer-dump: framebuffer-dump.c $(FBINK_DIR)/Release/libfbink.a | $(BUILD)
	$(CC) $(CFLAGS) $(FBINK_FLAGS) $< $(FBINK_DIR)/Release/libfbink.a -lm -o $@
	$(STRIP) --strip-unneeded $@

clean:
	rm -f $(BUILD)/close-fds-exec $(BUILD)/uinput-touch \
	      $(BUILD)/fbink-state $(BUILD)/framebuffer-dump
	rmdir $(BUILD) 2>/dev/null || true
