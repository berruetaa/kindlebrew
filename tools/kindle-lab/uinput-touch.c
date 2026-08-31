#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

static int write_all(int fd, const void *data, size_t size) {
    const unsigned char *p = data;
    while (size > 0) {
        const ssize_t written = write(fd, p, size);
        if (written > 0) {
            p += (size_t)written;
            size -= (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static int emit_event(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    return write_all(fd, &event, sizeof(event));
}

static int sync_frame(int fd) {
    return emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static int sleep_milliseconds(int milliseconds) {
    struct timespec remaining = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

static int emit_position(int fd, int x, int y) {
    if (emit_event(fd, EV_ABS, ABS_MT_POSITION_X, x) != 0 ||
        emit_event(fd, EV_ABS, ABS_MT_POSITION_Y, y) != 0 ||
        emit_event(fd, EV_ABS, ABS_X, x) != 0 ||
        emit_event(fd, EV_ABS, ABS_Y, y) != 0) {
        return -1;
    }
    return 0;
}

static int emit_contact_position(int fd, int x, int y) {
    /*
     * The input core suppresses an ABS report when it equals the device's
     * previous value.  A long-lived uinput device can therefore omit one
     * axis on the first frame seen by a newly launched app.  Prime both axes
     * with a different in-range value, then publish the requested position;
     * KBGE only consumes the final values at SYN_REPORT.
     */
    const int probe_x = x == 0 ? 1 : 0;
    const int probe_y = y == 0 ? 1 : 0;
    return emit_position(fd, probe_x, probe_y) != 0 ||
                   emit_position(fd, x, y) != 0
               ? -1
               : 0;
}

static int touch_down(int fd, int x, int y, int tracking_id) {
    if (emit_event(fd, EV_ABS, ABS_MT_SLOT, 0) != 0 ||
        emit_event(fd, EV_ABS, ABS_MT_TRACKING_ID, tracking_id) != 0 ||
        emit_contact_position(fd, x, y) != 0 ||
        emit_event(fd, EV_KEY, BTN_TOUCH, 1) != 0) {
        return -1;
    }
    return sync_frame(fd);
}

static int touch_move(int fd, int x, int y) {
    if (emit_event(fd, EV_ABS, ABS_MT_SLOT, 0) != 0 ||
        emit_position(fd, x, y) != 0) {
        return -1;
    }
    return sync_frame(fd);
}

static int touch_up(int fd) {
    if (emit_event(fd, EV_ABS, ABS_MT_SLOT, 0) != 0 ||
        emit_event(fd, EV_ABS, ABS_MT_TRACKING_ID, -1) != 0 ||
        emit_event(fd, EV_KEY, BTN_TOUCH, 0) != 0) {
        return -1;
    }
    return sync_frame(fd);
}

static int enable_capabilities(int fd) {
    static const int abs_codes[] = {
        ABS_X,
        ABS_Y,
        ABS_MT_SLOT,
        ABS_MT_TRACKING_ID,
        ABS_MT_POSITION_X,
        ABS_MT_POSITION_Y,
    };

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0 ||
        ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) != 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_ABS) != 0) {
        return -1;
    }
#ifdef UI_SET_PROPBIT
    if (ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT) != 0) return -1;
#endif
    for (size_t i = 0; i < sizeof(abs_codes) / sizeof(abs_codes[0]); ++i) {
        if (ioctl(fd, UI_SET_ABSBIT, abs_codes[i]) != 0) return -1;
    }
    return 0;
}

static int create_device(int width, int height) {
    const int fd = open("/dev/input/uinput", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    if (enable_capabilities(fd) != 0) {
        close(fd);
        return -1;
    }

    struct uinput_user_dev device;
    memset(&device, 0, sizeof(device));
    (void)snprintf(device.name, sizeof(device.name), "kindlebrew-qa-touch");
    device.id.bustype = BUS_VIRTUAL;
    device.id.vendor = 0x4b42;
    device.id.product = 0x5141;
    device.id.version = 1;

    device.absmin[ABS_X] = 0;
    device.absmax[ABS_X] = width - 1;
    device.absmin[ABS_Y] = 0;
    device.absmax[ABS_Y] = height - 1;
    device.absmin[ABS_MT_SLOT] = 0;
    device.absmax[ABS_MT_SLOT] = 0;
    device.absmin[ABS_MT_TRACKING_ID] = 0;
    device.absmax[ABS_MT_TRACKING_ID] = 65535;
    device.absmin[ABS_MT_POSITION_X] = 0;
    device.absmax[ABS_MT_POSITION_X] = width - 1;
    device.absmin[ABS_MT_POSITION_Y] = 0;
    device.absmax[ABS_MT_POSITION_Y] = height - 1;

    if (write_all(fd, &device, sizeof(device)) != 0 || ioctl(fd, UI_DEV_CREATE) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static bool coordinates_valid(int x, int y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s FIFO WIDTH HEIGHT\n", argv[0]);
        return 2;
    }

    const char *const fifo_path = argv[1];
    const int width = atoi(argv[2]);
    const int height = atoi(argv[3]);
    if (width <= 1 || height <= 1) {
        fprintf(stderr, "invalid dimensions\n");
        return 2;
    }

    struct stat status;
    if (stat(fifo_path, &status) != 0 || !S_ISFIFO(status.st_mode)) {
        fprintf(stderr, "command path is not a FIFO: %s\n", fifo_path);
        return 2;
    }

    const int uinput_fd = create_device(width, height);
    if (uinput_fd < 0) {
        fprintf(stderr, "uinput setup failed: %s\n", strerror(errno));
        return 1;
    }

    const int command_fd = open(fifo_path, O_RDWR | O_CLOEXEC);
    if (command_fd < 0) {
        fprintf(stderr, "open FIFO failed: %s\n", strerror(errno));
        (void)ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        return 1;
    }
    FILE *const commands = fdopen(command_fd, "r");
    if (!commands) {
        fprintf(stderr, "fdopen FIFO failed: %s\n", strerror(errno));
        close(command_fd);
        (void)ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        return 1;
    }

    printf("READY name=kindlebrew-qa-touch width=%d height=%d\n", width, height);
    fflush(stdout);

    char line[128];
    int tracking_id = 1;
    bool touching = false;
    int exit_code = 0;
    while (fgets(line, sizeof(line), commands)) {
        int x = 0;
        int y = 0;
        int duration_ms = 30;
        const int tap_fields = sscanf(line, "tap %d %d %d", &x, &y, &duration_ms);
        if (tap_fields == 2 || tap_fields == 3) {
            if (!coordinates_valid(x, y, width, height) || touching ||
                duration_ms < 1 || duration_ms > 10000 ||
                touch_down(uinput_fd, x, y, tracking_id++) != 0) {
                fprintf(stderr, "tap down failed\n");
                exit_code = 1;
                break;
            }
            if (sleep_milliseconds(duration_ms) != 0 || touch_up(uinput_fd) != 0) {
                fprintf(stderr, "tap up failed\n");
                exit_code = 1;
                break;
            }
            printf("TAP %d %d duration_ms=%d\n", x, y, duration_ms);
            fflush(stdout);
            continue;
        }
        if (sscanf(line, "down %d %d", &x, &y) == 2) {
            if (!coordinates_valid(x, y, width, height) || touching ||
                touch_down(uinput_fd, x, y, tracking_id++) != 0) {
                fprintf(stderr, "down failed\n");
                exit_code = 1;
                break;
            }
            touching = true;
            continue;
        }
        if (sscanf(line, "move %d %d", &x, &y) == 2) {
            if (!coordinates_valid(x, y, width, height) || !touching ||
                touch_move(uinput_fd, x, y) != 0) {
                fprintf(stderr, "move failed\n");
                exit_code = 1;
                break;
            }
            continue;
        }
        if (strcmp(line, "up\n") == 0 || strcmp(line, "up") == 0) {
            if (!touching || touch_up(uinput_fd) != 0) {
                fprintf(stderr, "up failed\n");
                exit_code = 1;
                break;
            }
            touching = false;
            continue;
        }
        if (strcmp(line, "quit\n") == 0 || strcmp(line, "quit") == 0) break;
        fprintf(stderr, "unknown command: %s", line);
    }

    if (touching) (void)touch_up(uinput_fd);
    fclose(commands);
    (void)ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    return exit_code;
}
