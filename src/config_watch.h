#ifndef CONFIG_WATCH_H
#define CONFIG_WATCH_H

#include <stddef.h>

struct miru_config_watch {
    int inotify_fd;
    int watch_wd;
    char config_dir[512];
    char config_filename[128];
};

int config_watch_init(struct miru_config_watch *watch);

int config_watch_get_fd(const struct miru_config_watch *watch);

int config_watch_check(struct miru_config_watch *watch);

void config_watch_cleanup(struct miru_config_watch *watch);

#endif // !CONFIG_WATCH_H
