#ifndef CONFIG_H
#define CONFIG_H

struct miru_config {
    // Zoom
    double zoom_factor;
    double zoom_increment;
    double zoom_max_factor;
    int zoom_smooth;

    // Spotlight
    long spotlight_radius;
    double spotlight_dim;
    long spotlight_softness;

    // General
    int show_cursor;
};

void config_load(struct miru_config *out);

#endif // !CONFIG_H
