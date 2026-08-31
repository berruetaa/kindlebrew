#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s PROGRAM [ARG ...]\n", argv[0]);
        return 2;
    }

    long limit = sysconf(_SC_OPEN_MAX);
    if (limit < 0 || limit > 65536) limit = 65536;
    for (int fd = 3; fd < limit; ++fd) {
        (void)close(fd);
    }

    execvp(argv[1], &argv[1]);
    fprintf(stderr, "exec %s: %s\n", argv[1], strerror(errno));
    return 126;
}
