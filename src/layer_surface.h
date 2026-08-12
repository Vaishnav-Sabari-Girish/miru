#ifndef LAYER_SURFACE_H
#define LAYER_SURFACE_H
#include <stddef.h>
#include <stdbool.h>
#include "wayland_state.h"
#include "capture.h"
#include "egl_context.h"
#include "gl_renderer.h"

struct layer_surface_config {
    float zoom_default;
    float zoom_max;
    float spotlight_radius;
    float spotlight_dim;
    float spotlight_softness;
    //};

    /* // one shm-backed wl_buffer and its own independent busy/release tracking */
    /* struct miru_buffer_slot { */
    /*     struct wl_buffer *buffer; */
    /*     void *shm_data; */
    /*     size_t shm_size; */
    /*     int busy; // 1 from the moment we attach+commit it until its own release event fires */
    bool smooth_enabled;
};

struct miru_layer_surface {
    /* struct wl_shm *shm; */
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    /* struct miru_buffer_slot slots[2]; // double buffering: alternate into whichever slot is free */
    /* int width; */
    /* int height; */
    /* int buffer_width; // actual allocated buffer size (capture's physical size) */
    /* int buffer_height; */
    struct miru_egl egl;
    struct miru_gl_renderer gl;

    int width, height;
    int buffer_width, buffer_height;
    int output_scale;
    bool configured;
    bool closed;
    const struct miru_capture *capture;
    /* int output_scale; */
    /* double cursor_x; */
    /* double cursor_y; */
    /* float zoom; */
    /* bool dirty; */
    /* float zoom_default; */
    /* float zoom_max; */
    /* float spotlight_radius; */
    /* float spotlight_dim; */
    /* float spotlight_softness; */
    double cursor_x, cursor_y;
    float zoom;

    double display_cursor_x, display_cursor_y;
    float display_zoom;

    float zoom_default, zoom_max;
    float spotlight_radius, spotlight_dim, spotlight_softness;
    bool spotlight_enabled;
    bool smooth_enabled;
    bool dirty;
};

int layer_surface_create(
    struct miru_state *state,
    struct miru_layer_surface *ls,
    const struct miru_capture *capture,
    /* // float zoom_default, */
    /* // float zoom_max */
    const struct layer_surface_config *config
);

void layer_surface_destroy(struct miru_layer_surface *ls);

void layer_surface_apply_config(struct miru_layer_surface *ls, const struct layer_surface_config *config);

// Interpolate towards the target zoom/cursor and redraws if needed
// no-op if fully settled and not dirty. Safe to call every main-loop iteration
void layer_surface_render(struct miru_layer_surface *ls);

// true while display_zoom/display_cursor haven't caught up to their targets
bool layer_surface_is_animating(const struct miru_layer_surface *ls);

#endif
