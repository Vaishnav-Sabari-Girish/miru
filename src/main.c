#include <stdio.h>
#include <signal.h>
#include "wayland_state.h"
#include "layer_surface.h"
#include "capture.h"

static volatile sig_atomic_t should_exit = 0;

static void handle_sigint(int sig) {
  (void)sig;
  should_exit = 1;
}

int main(void) {
  struct miru_state state = {0};
  struct miru_layer_surface ls = {0};

  struct sigaction sa = {0};
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;                                               // no SA_RESTART, poll() should see EINTR
  sigaction(SIGINT, &sa, NULL);

  if (wayland_state_init(&state) != 0) {
    return 1;
  }

  struct miru_capture capture = {0};
  if (capture_output_frame(&state, state.output, &capture) != 0) {
    fprintf(stderr, "screencopy capture failed\n");
  } else {
    fprintf(stderr, "captured frame: %ux%u stride = %u format = %u\n",
            capture.width, capture.height, capture.stride, capture.format
    );
  }
  capture_frame_destroy(&capture);

  if (layer_surface_create(&state, &ls) != 0) {
    fprintf(stderr, "failed to create layer surface\n");
    wayland_state_cleanup(&state);
    return 1;
  }

  fprintf(stderr, "layer surface created, entering event loop\n");
  state.running = 1;

  while (state.running && !should_exit && !ls.closed) {
    if (wayland_state_dispatch(&state) != 0) {
      break;
    }
  }

  fprintf(stderr, "shutting down\n");
  layer_surface_destroy(&ls);
  wayland_state_cleanup(&state);
  return 0;
}
