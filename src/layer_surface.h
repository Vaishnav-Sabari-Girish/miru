#ifndef LAYER_SURFACE_H
#define LAYER_SURFACE_H
#include <stddef.h>
#include "wayland_state.h"
#include "capture.h"

// one shm-backed wl_buffer and its own independent busy/release tracking
struct miru_buffer_slot {
    struct wl_buffer *buffer;
    void *shm_data;
    size_t shm_size;
    int busy; // 1 from the moment we attach+commit it until its own release event fires
};

struct miru_layer_surface {
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct miru_buffer_slot slots[2]; // real double buffering: alternate which one we write into
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
// re-blits ls -> capture into whichever buffer slot is currently free and re-commits
// call for updating ls -> capture with a fresh frame;
// no-op if not yet configured, both slots are still busy, or the new capture's
// dimensions do not match the existing buffers
// NOT currently called anywhere - kept in reserve for a possible future
// "refresh while active" IPC command. If that never materializes, this
// and blit_and_commit's split-out-from-handle_configure structure should
// just be removed rather than left as unreferenced API.
void layer_surface_render(struct miru_layer_surface *ls);
#endif
