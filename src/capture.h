#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include "wayland_state.h"

// one still-frame capture off the compositor, backed by its own shm memory
struct miru_capture {
    struct wl_buffer *buffer;
    void *shm_data;
    size_t shm_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride; // exact stride the compositor told us to use, may include padding
    uint32_t format; // WL_SHM_FORMAT_* the compositor actually gave us, not something we choose
    int y_invert; // 1 if this frame is delivered upside-down, blit code must flip rows
};

// blocks until the compositor delivers one full frame from `output`, returns -1 on failure
// cancel, if non-NULL, is checked between dispatch retries so shutdown can interrupt a stuck capture
// zero-initializes *out itself, safe to pass an uninitialized struct
int capture_output_frame(
    struct miru_state *state,
    struct wl_output *output,
    volatile sig_atomic_t *cancel,
    struct miru_capture *out
);

void capture_frame_destroy(struct miru_capture *capture);

#endif
