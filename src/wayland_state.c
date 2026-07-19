#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/poll.h>
#include <wayland-client-protocol.h>
#include "wayland_state.h"
#include "input.h"

#define WAYLAND_MIN(a, b) ((a) < (b) ? (a) : (b)) // clamp our desired version to whatever the compositor advertised

static void handle_output_geometry(
    void *data,
    struct wl_output *wl_output,
    int32_t x,
    int32_t y,
    int32_t physical_width,
    int32_t physical_height,
    int32_t subpixel,
    const char *make,
    const char *model,
    int32_t transform
)
{
    (void)data;
    (void)wl_output;
    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void handle_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    struct miru_state *state = data;
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        if (!state->pointer) {
            state->pointer = wl_seat_get_pointer(seat);
            input_attach_pointer_listener(state->pointer, state->input_ctx);
        }
    } else if (state->pointer) {
        wl_pointer_release(state->pointer);
        state->pointer = NULL;
    }
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        if (!state->keyboard) {
            state->keyboard = wl_seat_get_keyboard(seat);
            input_attach_keyboard_listener(state->keyboard, state->input_ctx);
        }
    } else if (state->keyboard) {
        wl_keyboard_release(state->keyboard);
        state->keyboard = NULL;
    }
}

static void handle_seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = handle_seat_capabilities,
    .name = handle_seat_name,
};

static void handle_output_mode(
    void *data,
    struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh
)
{
    (void)data;
    (void)wl_output;
    (void)flags;
    (void)width;
    (void)height;
    (void)refresh;
}

static void handle_output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
    (void)wl_output;
    struct miru_state *state = data;
    state->output_scale = factor;
    fprintf(stderr, "output scale: %d\n", factor);
}

static void handle_output_done(void *data, struct wl_output *wl_output)
{
    (void)data;
    (void)wl_output;
}

static void handle_output_name(void *data, struct wl_output *wl_output, const char *name)
{
    (void)data;
    (void)wl_output;
    (void)name;
}

static void handle_output_description(void *data, struct wl_output *wl_output, const char *description)
{
    (void)data;
    (void)wl_output;
    (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = handle_output_geometry,
    .mode = handle_output_mode,
    .done = handle_output_done,
    .scale = handle_output_scale,
    .name = handle_output_name,
    .description = handle_output_description,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    struct miru_state *state = data;
    fprintf(stderr, "global: %s (v%u)\n", interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, WAYLAND_MIN(version, 4));
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !state->seat) {
        state->seat = wl_registry_bind(registry, name, &wl_seat_interface, WAYLAND_MIN(version, 7));
        wl_seat_add_listener(state->seat, &seat_listener, state);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0 && !state->output) {
        state->output = wl_registry_bind(registry, name, &wl_output_interface, WAYLAND_MIN(version, 4));
        wl_output_add_listener(state->output, &output_listener, state);
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
    state->output_scale = 1;
    state->display = wl_display_connect(NULL);
    if (!state->display) {
        fprintf(stderr, "Failed to connect to wayland display, is a compositor running ?\n");
        return -1;
    }

    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);

    // wl_output's scale event isn't guaranteed to have arrived after the first
    // roundtrip (it's sent async right after bind), one more guarantee it has
    wl_display_roundtrip(state->display);

    if (!state->compositor || !state->seat || !state->shm || !state->layer_shell || !state->screencopy_manager) {
        fprintf(stderr, "compositor is missing a required protocol, check the log above\n");
        wl_display_disconnect(state->display);
        return -1;
    }
    return 0;
}

// int wayland_state_dispatch(struct miru_state *state, int timeout_ms)
// {
//     while (wl_display_prepare_read(state->display) != 0) {
//         wl_display_dispatch_pending(state->display);
//     }
//
//     int pending_write = 0;
//     if (wl_display_flush(state->display) == -1) {
//         if (errno == EAGAIN) {
//             pending_write = 1;
//         } else {
//             wl_display_cancel_read(state->display);
//             fprintf(stderr, "wl_display_flush failed\n");
//             return -1;
//         }
//     }
//
//     struct pollfd pfd = {
//         .fd = wl_display_get_fd(state->display),
//         .events = (short)(POLLIN | (pending_write ? POLLOUT : 0)),
//     };
//     int ret = poll(&pfd, 1, timeout_ms);
//     if (ret == -1) {
//         wl_display_cancel_read(state->display);
//         if (errno == EINTR) {
//             return 0;
//         }
//         fprintf(stderr, "poll failed\n");
//         return -1;
//     }
//
//     if (pfd.revents & POLLIN) {
//         wl_display_read_events(state->display);
//     } else {
//         wl_display_cancel_read(state->display);
//     }
//
//     if (pfd.revents & POLLOUT) {
//         if (wl_display_flush(state->display) == -1 && errno != EAGAIN) {
//             fprintf(stderr, "wl_display_flush failed after POLLOUT\n");
//             return -1;
//         }
//     }
//
//     wl_display_dispatch_pending(state->display);
//     return 0;
// }

int wayland_state_get_fd(struct miru_state *state)
{
    return wl_display_get_fd(state->display);
}

int wayland_state_prepare(struct miru_state *state, short *out_poll_events)
{
    while (wl_display_prepare_read(state->display) != 0) {
        wl_display_dispatch_pending(state->display);
    }

    short events = POLLIN;

    if (wl_display_flush(state->display) == -1) {
        if (errno == EAGAIN) {
            events |= POLLOUT;
        } else {
            wl_display_cancel_read(state->display);
            fprintf(stderr, "wl_display_flush failed\n");
            return -1;
        }
    }

    *out_poll_events = events;
    return 0;
}

int wayland_state_process(struct miru_state *state, short revents)
{
    if (revents & POLLIN) {
        wl_display_read_events(state->display);
    } else {
        wl_display_cancel_read(state->display);
    }

    if (revents & POLLOUT) {
        if (wl_display_flush(state->display) == -1 && errno != EAGAIN) {
            fprintf(stderr, "wl_display_flush failed after POLLOUT\n");
            return -1;
        }
    }
    wl_display_dispatch_pending(state->display);
    return 0;
}

void wayland_state_cancel_read(struct miru_state *state)
{
    if (state->display) {
        wl_display_cancel_read(state->display);
    }
}

void wayland_state_cleanup(struct miru_state *state)
{
    if (state->display) {
        wl_display_disconnect(state->display);
    }
}
