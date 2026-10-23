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
    double zoom_animation_speed;

    int zoom_upscale;

    // Spotlight
    long spotlight_radius;
    double spotlight_dim;
    long spotlight_softness;
    double spotlight_animation_speed;
    double spotlight_radius_step;

    // General
    bool show_cursor;

    // Annotate
    int annotate_default;
    double annotate_thickness;
    float annotate_color_r, annotate_color_b, annotate_color_g;
    float annotate_text_r, annotate_text_b, annotate_text_g;
    double annotate_text_size;

    // Loupe 0 = rect, 1 = ellipse, 2 = circle
    int loupe_shape;

    int export_format;

    int color_filter;

    bool status_badge;
};

void config_load(struct miru_config *out);

int config_get_watch_paths(char *dir, size_t dir_size, char *filename, size_t filename_size);

int config_gen_interactive(void);
#endif // !CONFIG_H
