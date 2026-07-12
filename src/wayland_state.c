#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include "wayland_state.h"

// called once per global the compositor advertises
// this is how to "discover" everything
static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) 
{
  struct miru_state *state = data;        // data is the pointer we pass in later
  // log every interface so that we can sanity-check against Niri's output 
  fprintf(stderr, "global: %s (v%u)\n", interface, version);

  // strcmp against each interface's generated name to know what we're looking at
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state -> compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state -> seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state -> shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, wl_output_interface.name) == 0 && !state -> output) {
    // only keep the first output for now, multi-monitor comes later
    state -> output = wl_registry_bind(registry, name, &wl_output_interface, 4);
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    state -> layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
    state -> screencopy_manager = wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 1);
  }

  // anything we do not recognize is just ignored, we do not need every global
}

// called is a global disappears at runtime
static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  // no-op for now
  (void)data;
  (void)registry;
  (void)name;
}


// bundles the two callbacks above into the struct libwayland expects
static const struct wl_registry_listener registry_listener = {
  .global = registry_global,
  .global_remove = registry_global_remove,
};

int wayland_state_init(struct miru_state *state) {
  state -> display = wl_display_connect(NULL);       // NULL = use $WAYLAND_DISPLAY
  if (!state -> display) {
    fprintf(stderr, "Failed to connect to wayland display, is a compositor running ?\n");
    return 1;
  }

  state -> registry = wl_display_get_registry(state -> display);               // ask for registry object
  wl_registry_add_listener(state -> registry, &registry_listener, state);  // hook the callbacks

  wl_display_roundtrip(state -> display);          // Block until the compositor has sent all the globals

  if (!state -> compositor || !state -> seat || !state -> shm ||
    !state -> layer_shell || !state -> screencopy_manager
  ) 
  {
    fprintf(stderr, "compositor is missing a required protocol, check the log above");
    wl_display_disconnect(state -> display);
    return -1;
  }

  return 0;
}

int wayland_state_dispatch(struct miru_state *state) {
  // must drain fully queued events before sleeping, or we could miss one that arrived earlier
  while (wl_display_prepare_read(state -> display) != 0) {
    wl_display_dispatch_pending(state -> display);
  }

  wl_display_flush(state -> display);    // send anything queued up to the compositor

  struct pollfd pfd = {
    .fd = wl_display_get_fd(state -> display),
    .events = POLLIN,
  };
  int ret = poll(&pfd, 1, -1);

  if (ret == -1) {
    wl_display_cancel_read(state -> display);   // must cancel, can't read after poll failed
    if (errno == EINTR) {
      return 0;                                 // Interrupted by a signal and not an error
    }
    fprintf(stderr, "poll failed\n");

    return -1;
  }

  if (pfd.revents & POLLIN) {
    wl_display_read_events(state -> display);
  } else {
    wl_display_cancel_read(state -> display);
  }

  wl_display_dispatch_pending(state -> display);
  return 0;
}

void wayland_state_cleaner(struct miru_state *state) {
  if (state -> display) {
    wl_display_disconnect(state -> display);
  }
}
