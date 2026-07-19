#ifndef LAYER_SURFACE_H
#define LAYER_SURFACE_H

#include <stddef.h>
#include "wayland_state.h"
#include "capture.h"

struct miru_layer_surface {
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_buffer *buffer;
    void *shm_data;
    size_t shm_size;
    int width;
    int height;
    int buffer_width; // actual allocated buffer size (capture's physical size)
    int buffer_height;
    int configured;
    int closed; // set by handle_closed, main's loop checks this to exit cleanly
    const struct miru_capture *capture;
    int output_scale;
    double cursor_x;
    double cursor_y;
    float zoom;
    int dirty;
};

int layer_surface_create(struct miru_state *state, struct miru_layer_surface *ls, const struct miru_capture *capture);
void layer_surface_destroy(struct miru_layer_surface *ls);

// re-blits ls -> capture into the already allocated buffer and re-commits
// call for updating ls -> capture with a fresh frame;
// no-op if not yet configured, or if the new capture's dimensions do not match the existing one
// NOT currently called anywhere - kept in reserve for a possible future
// "refresh while active" IPC command. If that never materializes, this
// and blit_and_commit's spit-out-from-handle_configure structure should
// just be removed rather than left as unreferenced API.
void layer_surface_render(struct miru_layer_surface *ls);

#endif
