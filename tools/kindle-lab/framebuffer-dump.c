#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fbink.h"

static int write_all(int fd, const uint8_t *bytes, size_t size) {
    while (size > 0) {
        const ssize_t written = write(fd, bytes, size);
        if (written > 0) {
            bytes += (size_t)written;
            size -= (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT.raw\n", argv[0]);
        return 2;
    }

    FBInkConfig config;
    memset(&config, 0, sizeof(config));
    config.is_quiet = true;
    config.no_viewport = true;

    const int fbfd = fbink_open();
    if (fbfd < 0) {
        fprintf(stderr, "FBInk open failed\n");
        return 1;
    }
    if (fbink_init(fbfd, &config) < 0) {
        fprintf(stderr, "FBInk initialization failed\n");
        (void)fbink_close(fbfd);
        return 1;
    }

    FBInkState state;
    memset(&state, 0, sizeof(state));
    fbink_get_state(&config, &state);

    size_t mapped_size = 0;
    const uint8_t *const pixels = fbink_get_fb_pointer(fbfd, &mapped_size);
    if (state.screen_height != 0 &&
        (size_t)state.scanline_stride > SIZE_MAX / (size_t)state.screen_height) {
        fprintf(stderr, "framebuffer dimensions overflow\n");
        (void)fbink_close(fbfd);
        return 1;
    }
    const size_t required = (size_t)state.scanline_stride * state.screen_height;
    if (!pixels || state.pixel_format != FBINK_PXFMT_Y8 ||
        state.view_hori_origin != 0 || state.view_vert_origin != 0 ||
        state.scanline_stride < state.screen_width || required > mapped_size) {
        fprintf(stderr, "unsupported framebuffer layout\n");
        (void)fbink_close(fbfd);
        return 1;
    }

    uint8_t *first = malloc(required);
    uint8_t *second = malloc(required);
    if (!first || !second) {
        fprintf(stderr, "snapshot allocation failed\n");
        free(first);
        free(second);
        (void)fbink_close(fbfd);
        return 1;
    }

    int stable_attempt = 0;
    const struct timespec settle = {.tv_sec = 0, .tv_nsec = 15000000L};
    for (int attempt = 1; attempt <= 8; ++attempt) {
        memcpy(first, pixels, required);
        (void)nanosleep(&settle, NULL);
        memcpy(second, pixels, required);
        if (memcmp(first, second, required) == 0) {
            stable_attempt = attempt;
            break;
        }
    }
    free(first);
    if (stable_attempt == 0) {
        fprintf(stderr, "framebuffer did not stabilize after 8 bounded attempts\n");
        free(second);
        (void)fbink_close(fbfd);
        return 1;
    }

    if (access(argv[1], F_OK) == 0) {
        fprintf(stderr, "refusing to overwrite existing snapshot: %s\n", argv[1]);
        free(second);
        (void)fbink_close(fbfd);
        return 1;
    }

    const size_t temp_length = strlen(argv[1]) + 32U;
    char *temp = malloc(temp_length);
    const int temp_written = temp
        ? snprintf(temp, temp_length, "%s.tmp.%ld", argv[1], (long)getpid())
        : -1;
    if (!temp || temp_written < 0 || (size_t)temp_written >= temp_length) {
        fprintf(stderr, "snapshot temporary path failed\n");
        free(temp);
        free(second);
        (void)fbink_close(fbfd);
        return 1;
    }

    const int output = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (output < 0) {
        fprintf(stderr, "open snapshot: %s\n", strerror(errno));
        free(temp);
        free(second);
        (void)fbink_close(fbfd);
        return 1;
    }

    int result = 0;
    for (unsigned int y = 0; y < state.screen_height; ++y) {
        if (write_all(output, second + (size_t)y * state.scanline_stride,
                      state.screen_width) != 0) {
            result = 1;
            break;
        }
    }
    if (result == 0 && fsync(output) != 0) result = 1;
    if (close(output) != 0) result = 1;
    if (result == 0 && rename(temp, argv[1]) != 0) result = 1;
    if (result != 0) {
        fprintf(stderr, "write snapshot: %s\n", strerror(errno));
        (void)unlink(temp);
    }

    free(temp);
    free(second);
    if (fbink_close(fbfd) < 0) result = 1;
    if (result == 0) {
        printf("dumped %ux%u Y8 stable_attempt=%d\n",
               state.screen_width, state.screen_height, stable_attempt);
    }
    return result;
}
