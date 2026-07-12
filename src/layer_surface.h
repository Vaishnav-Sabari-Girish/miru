#ifndef LAYER_SURFACE_H
#define LAYER_SURFACE_H

#include <stddef.h>
#include "wayland_state.h"

struct miru_layer_surface {
    struct wl_shm *shm;                        // stashed here so the configure callback can (re)build buffers
    struct wl_surface *surface;                 // the base wayland surface
    struct zwlr_layer_surface_v1 *layer_surface; // the layer-shell "role" attached to it
    struct wl_buffer *buffer;
    void *shm_data;
    size_t shm_size;
    int width;
    int height;
    int configured;                             // set once the compositor tells us our actual size
};

// creates a fullscreen overlay surface on the "overlay" layer
int layer_surface_create(struct miru_state *state, struct miru_layer_surface *ls);

void layer_surface_destroy(struct miru_layer_surface *ls);

#endif // !LAYER_SURFACE_H
