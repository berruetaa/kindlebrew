/*
 * Kindlebrew Game Engine utilities: RNG and persistent storage.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "kb_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

void kb_rng_seed(KBGame *game, uint64_t seed) {
    if (!game) return;

    if (seed == 0) {
        uint64_t entropy = 0;
        int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            ssize_t got = read(fd, &entropy, sizeof(entropy));
            close(fd);
            if (got != (ssize_t)sizeof(entropy)) entropy = 0;
        }

        if (entropy == 0) {
            entropy = kb_now_ms();
            entropy ^= (uint64_t)(unsigned long)getpid() << 32;
            entropy ^= (uint64_t)(uintptr_t)game;
        }
        seed = entropy;
    }

    uint64_t s = seed;
    game->rng_state = splitmix64(&s);
    if (game->rng_state == 0) game->rng_state = UINT64_C(0xA5A5A5A55A5A5A5A);
}

uint32_t kb_random_u32(KBGame *game) {
    if (!game) return 0;
    if (game->rng_state == 0) kb_rng_seed(game, 0);

    /* xorshift64*: tiny, deterministic and more than sufficient for games. */
    uint64_t x = game->rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    game->rng_state = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32);
}

uint32_t kb_random_range(KBGame *game, uint32_t upper_exclusive) {
    if (!upper_exclusive) return 0;

    /* Lemire-style rejection threshold avoids modulo bias. */
    uint32_t threshold = (uint32_t)(-upper_exclusive) % upper_exclusive;
    for (;;) {
        uint32_t r = kb_random_u32(game);
        if (r >= threshold) return r % upper_exclusive;
    }
}

static bool safe_component(const char *s) {
    if (!s || !*s) return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '.' || *p == '_' || *p == '-') {
            continue;
        }
        return false;
    }
    return strcmp(s, ".") != 0 && strcmp(s, "..") != 0;
}

static int ensure_dir(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
    return -1;
}

int kb_data_path(KBGame *game, const char *filename, char *out, size_t out_size) {
    if (!game || !out || out_size == 0 || !safe_component(game->config.app_id) ||
        !safe_component(filename)) {
        if (game) kb_set_error(game, "invalid app_id or persistent-data filename");
        return -1;
    }

#ifdef KB_KINDLE
    const char *root = "/mnt/us/kindlebrew-data";
#else
    const char *root = ".kbgame-data";
#endif

    if (ensure_dir(root) != 0) {
        kb_set_error(game, "cannot create data root %s: %s", root, strerror(errno));
        return -1;
    }

    char app_dir[384];
    int n = snprintf(app_dir, sizeof(app_dir), "%s/%s", root, game->config.app_id);
    if (n < 0 || (size_t)n >= sizeof(app_dir)) {
        kb_set_error(game, "persistent-data path is too long");
        return -1;
    }
    if (ensure_dir(app_dir) != 0) {
        kb_set_error(game, "cannot create app data directory %s: %s", app_dir, strerror(errno));
        return -1;
    }

    n = snprintf(out, out_size, "%s/%s", app_dir, filename);
    if (n < 0 || (size_t)n >= out_size) {
        kb_set_error(game, "persistent-data path is too long");
        return -1;
    }
    return 0;
}

static int write_all(int fd, const unsigned char *data, size_t size) {
    while (size) {
        ssize_t n = write(fd, data, size);
        if (n > 0) {
            data += (size_t)n;
            size -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static void fsync_parent_best_effort(const char *path) {
    if (!path) return;
    char dir[512];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(dir)) return;
    memcpy(dir, path, n + 1U);

    char *slash = strrchr(dir, '/');
    if (!slash) {
        strcpy(dir, ".");
    } else if (slash == dir) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }

    int dfd = open(dir, O_RDONLY | O_CLOEXEC);
    if (dfd >= 0) {
        /*
         * Some FAT-backed Kindle userstores reject directory fsync with
         * EINVAL. The file itself was already fsync'ed; this is an extra
         * durability fence where the filesystem supports it.
         */
        (void)fsync(dfd);
        close(dfd);
    }
}

int kb_save_atomic(const char *path, const void *data, size_t size) {
    if (!path || !*path || (!data && size)) return -1;

    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(tmp)) return -1;

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return -1;

    int rc = 0;
    if (write_all(fd, (const unsigned char *)data, size) != 0) rc = -1;
    if (rc == 0 && fsync(fd) != 0) rc = -1;
    if (close(fd) != 0) rc = -1;

    if (rc == 0) {
        if (rename(tmp, path) != 0) {
            rc = -1;
        } else {
            fsync_parent_best_effort(path);
        }
    }
    if (rc != 0) unlink(tmp);
    return rc;
}

void *kb_load_file(const char *path, size_t *size_out) {
    if (size_out) *size_out = 0;
    if (!path || !*path) {
        errno = EINVAL;
        return NULL;
    }

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 0) {
        close(fd);
        return NULL;
    }

    size_t size = (size_t)st.st_size;
    if (size > 16U * 1024U * 1024U) {
        close(fd);
        errno = EFBIG;
        return NULL;
    }

    unsigned char *buf = malloc(size ? size : 1U);
    if (!buf) {
        close(fd);
        return NULL;
    }

    size_t off = 0;
    while (off < size) {
        ssize_t n = read(fd, buf + off, size - off);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n == 0) errno = EIO;
        free(buf);
        close(fd);
        return NULL;
    }

    close(fd);
    if (size_out) *size_out = size;
    return buf;
}

void kb_free(void *ptr) {
    free(ptr);
}
