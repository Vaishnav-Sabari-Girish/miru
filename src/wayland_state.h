#ifndef WAYLAND_STATE_H
#define WAYLAND_STATE_H

#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

struct miru_state {
    struct wl_display *display;
    struct wl_registry *registry;

    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct wl_shm *shm;
    struct wl_output *output;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;

    int running;
};

int wayland_state_init(struct miru_state *state);
int wayland_state_dispatch(struct miru_state *state);
void wayland_state_cleanup(struct miru_state *state);

#endif
