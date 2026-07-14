#ifndef LAYER_SURFACE_H
#define LAYER_SURFACE_H

#include <stddef.h>
#include "wayland_state.h"

struct miru_layer_surface {
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_buffer *buffer;
    void *shm_data;
    size_t shm_size;
    int width;
    int height;
    int configured;
    int closed; // set by handle_closed, main's loop checks this to exit cleanly
};

int layer_surface_create(struct miru_state *state, struct miru_layer_surface *ls);
void layer_surface_destroy(struct miru_layer_surface *ls);

#endif
