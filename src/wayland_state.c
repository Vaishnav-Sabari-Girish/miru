#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include "wayland_state.h"

#define WAYLAND_MIN(a, b) ((a) < (b) ? (a) : (b)) // clamp our desired version to whatever the compositor advertised

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    struct miru_state *state = data;
    fprintf(stderr, "global: %s (v%u)\n", interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, WAYLAND_MIN(version, 4));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = wl_registry_bind(registry, name, &wl_seat_interface, WAYLAND_MIN(version, 7));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0 && !state->output) {
        state->output = wl_registry_bind(registry, name, &wl_output_interface, WAYLAND_MIN(version, 4));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        state->screencopy_manager =
            wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, WAYLAND_MIN(version, 1));
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int wayland_state_init(struct miru_state *state)
{
    state->display = wl_display_connect(NULL);
    if (!state->display) {
        fprintf(stderr, "Failed to connect to wayland display, is a compositor running ?\n");
        return -1;
    }

    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);

    if (!state->compositor || !state->seat || !state->shm || !state->layer_shell || !state->screencopy_manager) {
        fprintf(stderr, "compositor is missing a required protocol, check the log above\n");
        wl_display_disconnect(state->display);
        return -1;
    }
    return 0;
}

int wayland_state_dispatch(struct miru_state *state)
{
    while (wl_display_prepare_read(state->display) != 0) {
        wl_display_dispatch_pending(state->display);
    }

    int pending_write = 0;
    if (wl_display_flush(state->display) == -1) {
        if (errno == EAGAIN) {
            pending_write = 1;
        } else {
            wl_display_cancel_read(state->display);
            fprintf(stderr, "wl_display_flush failed\n");
            return -1;
        }
    }

    struct pollfd pfd = {
        .fd = wl_display_get_fd(state->display),
        .events = (short)(POLLIN | (pending_write ? POLLOUT : 0)),
    };
    int ret = poll(&pfd, 1, -1);
    if (ret == -1) {
        wl_display_cancel_read(state->display);
        if (errno == EINTR) {
            return 0;
        }
        fprintf(stderr, "poll failed\n");
        return -1;
    }

    if (pfd.revents & POLLIN) {
        wl_display_read_events(state->display);
    } else {
        wl_display_cancel_read(state->display);
    }

    if (pfd.revents & POLLOUT) {
        if (wl_display_flush(state->display) == -1 && errno != EAGAIN) {
            fprintf(stderr, "wl_display_flush failed after POLLOUT\n");
            return -1;
        }
    }

    wl_display_dispatch_pending(state->display);
    return 0;
}

void wayland_state_cleanup(struct miru_state *state)
{
    if (state->display) {
        wl_display_disconnect(state->display);
    }
}
