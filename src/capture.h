#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>
#include <stdint.h>
#include "wayland_state.h"

// one still-frame capture off the compositor, backed by shm memory as own
struct miru_capture {
  struct wl_buffer *buffer;
  void *shm_data;
  size_t shm_size;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t format;           // WL_SHM_FORMAT_* the compositor actually gave us, not something we choose
};

// blocks until the compositor delivers one full frame from output 
int capture_output_frame(struct miru_state *state, struct wl_output *output, struct miru_capture *out);

void capture_frame_destroy(struct miru_capture *capture);

#endif // !CAPTURE_H
