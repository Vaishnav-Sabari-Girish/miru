#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

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

#endif // !CONFIG_H
