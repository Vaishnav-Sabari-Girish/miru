#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

struct miru_config {
    // Zoom
    double zoom_factor;
    double zoom_increment;
    double zoom_max_factor;
    bool zoom_smooth;

    // Spotlight
    long spotlight_radius;
    double spotlight_dim;
    long spotlight_softness;

    // General
    bool show_cursor;
};

void config_load(struct miru_config *out);

int config_get_watch_paths(char *dir, size_t dir_size, char *filename, size_t filename_size);
#endif // !CONFIG_H
