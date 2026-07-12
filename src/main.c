#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "wayland_state.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include <errno.h>
#include <poll.h>

// SIGINT sets this to break the loop,
// since we're not allowed to touch miru_state directly from a signal handler
static volatile sig_atomic_t should_exit = 0;

static void handle_sigint(int sig) {
  (void)sig;           // Unused. Silences a compiler warning
  should_exit = 1;
}

// called once per global the compositor advertises, this is how we "discover" everything
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

int main(void) {
  struct miru_state state = {0};     // zero-initialize everything, all pointers start NULL

  struct sigaction sa = {0};
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;                  // set no SA_RESTART, so blocking syscalls return EINTR
  sigaction(SIGINT, &sa, NULL);

  state.display = wl_display_connect(NULL);     // NULL means "use $WAYLAND_DISPLAY", the normal case
  if (!state.display) {
    fprintf(stderr, "Failed to connect to wayland display, is a compositor running ?\n");
    return 1;
  }

  state.registry = wl_display_get_registry(state.display);               // ask for registry object
  wl_registry_add_listener(state.registry, &registry_listener, &state);  // hook the callbacks

  wl_display_roundtrip(state.display);          // Block until the compositor has sent all the globals

  // sanity check: bail loudly if something I need is not advertised
  if (!state.compositor || !state.seat || !state.layer_shell || !state.screencopy_manager) {
    fprintf(stderr, "Compositor is missing a required protocol, check the log above \n");
    wl_display_disconnect(state.display);
    return 1;
  }

  fprintf(stderr, "All required globals bound, entering event loop\n");
  state.running = 1;

  int display_fd = wl_display_get_fd(state.display);   // the raw fd behind the Wayland connection

  while (state.running && !should_exit) {
    // must fully drain queued events before sleeping, or we could miss one that arrived earlier
    while (wl_display_prepare_read(state.display) != 0) {
      wl_display_dispatch_pending(state.display);
    }
    wl_display_flush(state.display);              // send anything we've queued up to the compositor

    struct pollfd pfd = { .fd = display_fd, .events = POLLIN };
    int ret = poll(&pfd, 1, -1);                   // this is OUR syscall now, EINTR reaches us directly

    if (ret == -1) {
      wl_display_cancel_read(state.display);     // must cancel, we can't read after poll failed
      if (errno == EINTR) {
        continue;                              // loop back, should_exit gets checked at top
      }
      fprintf(stderr, "poll failed\n");
      break;
    }

    if (pfd.revents & POLLIN) {
      wl_display_read_events(state.display);     // actually read what the compositor sent
    } else {
      wl_display_cancel_read(state.display);     // nothing to read, back out cleanly
    }

    wl_display_dispatch_pending(state.display);    // run the callbacks (registry_global etc.) on what we read
  }

  fprintf(stderr, "Shutting Down\n");
  wl_display_disconnect(state.display);
  return 0;
}
