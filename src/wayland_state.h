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

  int running;
};

#endif
