#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include "capture.h"
#include "shm_buffer.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

// tracks one in-flight capture across it's async callback sequence
struct capture_ctx {
  struct miru_state *state;
  struct miru_capture *out;
  int done;                  // set once ready or failed fires, tells the wait loop below to stop
  int ok;                    // 1 = ready fired (pixels valid), 0 = failed fired
};

// first event: compositor tells the format/size/stride it's about to hand back
static void handle_buffer(
  void *data,
  struct zwlr_screencopy_frame_v1 *frame,
  uint32_t format,
  uint32_t width,
  uint32_t height,
  uint32_t stride
) {
  struct capture_ctx *ctx = data;

  ctx -> out -> format = format;
  ctx -> out -> width = width;
  ctx -> out -> height = height;
  ctx -> out -> stride = stride;

  void *pixels = NULL;
  size_t size = 0;

  // reuse the same shm_helper the layer_surface uses, screencopy needs an identical kind of buffer
  ctx -> out -> buffer = shm_buffer_create(ctx -> state -> shm, (int)width, (int)height, format, &pixels, &size);
  if (!ctx -> out -> buffer) {
    fprintf(stderr, "capture: failed to allocate shm buffer for frame\n");
    ctx -> done = 1;
    ctx -> ok = 0;
    return;
  }

  ctx -> out -> shm_data = pixels;
  ctx -> out -> shm_size = size;

  // we bound version 1 of this protocol, so there's no buffer_done to wait for,
  // hand the buffer straight back as soon as we know what shape it needs to be
  zwlr_screencopy_frame_v1_copy(frame, ctx -> out -> buffer);
}

static void handle_flags(
  void *data,
  struct zwlr_screencopy_frame_v1 *frame,
  uint32_t flags
) {
  (void)frame;
  (void)data;
  (void)flags;
}

static void handle_ready(
  void *data,
  struct zwlr_screencopy_frame_v1 *frame,
  uint32_t tv_sec_hi,
  uint32_t tv_sec_lo,
  uint32_t tv_nsec
) {
  (void)frame;
  (void)tv_sec_hi;
  (void)tv_sec_lo;
  (void)tv_nsec;
  
  struct capture_ctx *ctx = data;
  ctx -> done = 1;
  ctx -> ok = 1;
}

static void handle_failed(void *data, struct zwlr_screencopy_frame_v1 *frame) {
  (void)frame;
  struct capture_ctx *ctx = data;
  fprintf(stderr, "capture: compositor reported capture failed\n");
  ctx -> done = 1;
  ctx -> ok = 0;
}

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
  .buffer = handle_buffer,
  .flags = handle_flags,
  .ready = handle_ready,
  .failed = handle_failed,
};

int capture_output_frame(struct miru_state *state, struct wl_output *output, struct miru_capture *out) {
  struct capture_ctx ctx = {
    .state = state,
    .out = out,
    .done = 0,
    .ok = 0
  };

  // overlay_cursor = 0; don't bake the cursorr into the frame, draw our own
  struct zwlr_screencopy_frame_v1 *frame = 
    zwlr_screencopy_manager_v1_capture_output(state -> screencopy_manager, 0, output);

  if (!frame) {
    fprintf(stderr, "capture: failed to create screencopy frame\n");
    return -1;
  }

  zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, &ctx);

  // one-shot blocking capture, deliberately separate from the daemon's main poll loop for now,
  // wl_display_dispatch blocks until *some* event arrives, looping it until our flag flips
  // is the simplest correct way to wait for this specific multi-event sequence
  while (!ctx.done) {
    if (wl_display_dispatch(state -> display) == -1) {
      if (errno == EINTR) {
        continue;
      }
      fprintf(stderr, "capture: display dispatch failed while waiting for frame\n");
      zwlr_screencopy_frame_v1_destroy(frame);
      return -1;
    }
  }

  zwlr_screencopy_frame_v1_destroy(frame);

  if (!ctx.ok) {
    capture_frame_destroy(out);
    return -1;
  }

  return 0;
}

void capture_frame_destroy(struct miru_capture *capture) {
  if (capture -> shm_data) {
    shm_buffer_free(capture -> shm_data, capture -> shm_size);
    capture -> shm_data = NULL;
    capture -> shm_size = 0;
  }
  if (capture -> buffer) {
    wl_buffer_destroy(capture -> buffer);
    capture -> buffer = NULL;
  }
}
