#ifndef WAYLAND_STATE_H
#define WAYLAND_STATE_H

#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"     // replaces the ext-image-copy-capture header

struct miru_state {
  struct wl_display *display;
  struct wl_registry *registry;

  struct wl_compositor *compositor;
  struct wl_seat *seat;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;   // renamed from capture_manager

  struct wl_shm *shm;                        // needed to allocate the pixel buffer we hand to the compositor

  struct wl_output *output;

  struct wl_surface *surface;               // the actual drawable surface
  struct zwlr_layer_surface_v1 *layer_surface;   // layer_shell wrapper

  uint32_t surface_width;
  uint32_t surface_height;
  int configured;


  int running;
};

// connects to the display and binds every required global. Return 0 on success
int wayland_state_init(struct miru_state *state);

// runs one iteration of the poll-based event loop, 0 = keep going, -1 = stop (real error)
int wayland_state_dispatch(struct miru_state *state);

// disconnects cleanly
void wayland_state_cleaner(struct miru_state *state);

#endif
