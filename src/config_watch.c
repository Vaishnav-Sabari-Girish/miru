#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/inotify.h>
#include "config_watch.h"
#include "config.h"

int config_watch_init(struct miru_config_watch *watch)
{
    watch->inotify_fd = -1;
    watch->watch_wd = -1;

    if (config_get_watch_paths(
            watch->config_dir, sizeof(watch->config_dir), watch->config_filename, sizeof(watch->config_filename)
        ) != 0) {
        fprintf(stderr, "config_watch: unable to determine config directory, hot-reload disabled\n");
        return -1;
    }

    watch->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (watch->inotify_fd < 0) {
        perror("config_watch: inotify_init1");
        return -1;
    }

    // Watch the config directory and not the file
    // This is incase an app writes to a temp file and then overwrites the main once saved
    watch->watch_wd = inotify_add_watch(watch->inotify_fd, watch->config_dir, IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);

    if (watch->watch_wd < 0) {
        fprintf(
            stderr, "config_watch: unable to watch %s, hot-reload disabled: %s\n", watch->config_dir, strerror(errno)
        );

        close(watch->inotify_fd);
        watch->inotify_fd = -1;
        return -1;
    }

    fprintf(stderr, "config_watch: watching %s for changes to %s\n", watch->config_dir, watch->config_filename);
    return 0;
}

int config_watch_get_fd(const struct miru_config_watch *watch)
{
    return watch->inotify_fd;
}

int config_watch_check(struct miru_config_watch *watch)
{
    if (watch->inotify_fd < 0) {
        return 0;
    }

    char buf[4096] __attribute__((aligned(__alignof(struct inotify_event))));
    int changed = 0;

    for (;;) {
        ssize_t n = read(watch->inotify_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            perror("config_watch: read");
            return -1;
        }
        if (n == 0) {
            break;
        }

        ssize_t offset = 0;
        while (offset < n) {
            struct inotify_event *ev = (struct inotify_event *)(buf + offset);
            if (ev->len > 0 && strcmp(ev->name, watch->config_filename) == 0) {
                changed = 1;
            }
            offset += (ssize_t)sizeof(struct inotify_event) + ev->len;
        }
    }

    return changed;
}

void config_watch_cleanup(struct miru_config_watch *watch)
{
    if (watch->inotify_fd >= 0) {
        close(watch->inotify_fd);
        watch->inotify_fd = -1;
    }
}
