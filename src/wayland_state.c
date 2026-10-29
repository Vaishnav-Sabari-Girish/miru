#include "debug.h"
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
    struct miru_output *output = data;
    output->scale = factor;
    // fprintf(stderr, "output scale (wl_output integer): %d\n", factor);
    MIRU_LOG("output scale (wl_output integer): %d", factor);
}

static void handle_output_done(void *data, struct wl_output *wl_output)
{
    (void)data;
    (void)wl_output;
}

static void handle_output_name(void *data, struct wl_output *wl_output, const char *name)
{
    (void)wl_output;
    struct miru_output *output = data;
    free(output->name);
    output->name = strdup(name);
    MIRU_LOG("output name: %s", name);
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

static void output_destroy(struct miru_output *output)
{
    wl_list_remove(&output->link);
    if (wl_output_get_version(output->wl_output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
        wl_output_release(output->wl_output);
    } else {
        wl_output_destroy(output->wl_output);
    }
    free(output->name);
    free(output);
}

static const char *output_label(const struct miru_output *output)
{
    return output->name ? output->name : "(unnamed)";
}

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    struct miru_state *state = data;
    // fprintf(stderr, "global: %s (v%u)\n", interface, version);
    MIRU_LOG("global: %s (v%u)", interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, WAYLAND_MIN(version, 4));
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !state->seat) {
        state->seat = wl_registry_bind(registry, name, &wl_seat_interface, WAYLAND_MIN(version, 7));
        wl_seat_add_listener(state->seat, &seat_listener, state);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct miru_output *output = calloc(1, sizeof(*output));
        if (!output)
            return;
        output->global_name = name;
        output->scale = 1;
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, WAYLAND_MIN(version, 4));
        wl_output_add_listener(output->wl_output, &output_listener, output);
        // append, so the first advertised output stays first and remains the fallback
        wl_list_insert(state->outputs.prev, &output->link);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        state->screencopy_manager =
            wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        state->cursor_shape_manager =
            wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, WAYLAND_MIN(version, 1));
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        state->viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
        state->fractional_scale_manager =
            wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)registry;
    struct miru_state *state = data;
    struct miru_output *output;
    struct miru_output *tmp;
    wl_list_for_each_safe(output, tmp, &state->outputs, link)
    {
        if (output->global_name == name) {
            MIRU_LOG("output removed: %s", output_label(output));
            if (state->overlay_output == output)
                state->overlay_output = NULL;
            output_destroy(output);
            return;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int wayland_state_init(struct miru_state *state)
{
    wl_list_init(&state->outputs);
    state->display = wl_display_connect(NULL);
    if (!state->display) {
        // fprintf(stderr, "Failed to connect to wayland display, is a compositor running ?\n");
        MIRU_LOG("Failed to connect to wayland display, is a compositor running ?");
        return -1;
    }

    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);

    // wl_output's scale event isn't guaranteed to have arrived after the first
    // roundtrip (it's sent async right after bind), one more guarantee it has
    wl_display_roundtrip(state->display);

    if (!state->compositor || !state->seat || !state->shm || !state->layer_shell || !state->screencopy_manager) {
        // fprintf(stderr, "compositor is missing a required protocol, check the log above\n");
        MIRU_LOG("compositor is missing a required protocol, check the log above");
        wl_display_disconnect(state->display);
        return -1;
    }
    return 0;
}

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
            // fprintf(stderr, "wl_display_flush failed\n");
            MIRU_LOG("wl_display_flush failed");
            return -1;
        }
    }

    *out_poll_events = events;
    return 0;
}

int wayland_state_process(struct miru_state *state, short revents)
{
    if (revents & (POLLHUP | POLLERR)) {
        wl_display_cancel_read(state->display);
        // fprintf(stderr, "wayland_state_process: POLLHUP/POLLERR on display fd. connection lost\n");
        MIRU_LOG("wayland_state_process: POLLHUP/POLLERR on display fd. connection lost");
        return -1;
    }

    if (revents & POLLIN) {
        wl_display_read_events(state->display);
    } else {
        wl_display_cancel_read(state->display);
    }

    if (revents & POLLOUT) {
        if (wl_display_flush(state->display) == -1 && errno != EAGAIN) {
            // fprintf(stderr, "wl_display_flush failed after POLLOUT\n");
            MIRU_LOG("wl_display_flush failed after POLLOUT");
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

struct output_probe {
    struct miru_state *state;
    // registry name instead of a pointer because wl_display_roundtrip can dispatch
    // a global_remove which would free the output
    uint32_t entered_name;
    bool entered;
};

static struct miru_output *find_output(struct miru_state *state, struct wl_output *wl_output)
{
    struct miru_output *output;
    wl_list_for_each(output, &state->outputs, link)
    {
        if (output->wl_output == wl_output)
            return output;
    }
    return NULL;
}

static void probe_surface_enter(void *data, struct wl_surface *surface, struct wl_output *wl_output)
{
    (void)surface;
    struct output_probe *probe = data;
    const struct miru_output *output = find_output(probe->state, wl_output);
    if (output) {
        probe->entered_name = output->global_name;
        probe->entered = true;
    }
}

static void probe_surface_leave(void *data, struct wl_surface *surface, struct wl_output *wl_output)
{
    (void)surface;
    struct output_probe *probe = data;
    const struct miru_output *output = find_output(probe->state, wl_output);
    if (output && probe->entered && output->global_name == probe->entered_name)
        probe->entered = false;
}

static const struct wl_surface_listener probe_surface_listener = {
    .enter = probe_surface_enter,
    .leave = probe_surface_leave,
};

struct miru_output *wayland_state_focused_output(struct miru_state *state)
{
    if (wl_list_empty(&state->outputs))
        return NULL;

    // a layer surface created without an output is placed by the compositor, usually on the focused output
    // if a compositor did not not send an wl_surface.enter, fallback to using the first output
    struct output_probe probe = { .state = state, .entered_name = 0, .entered = false };
    struct wl_surface *surface = wl_compositor_create_surface(state->compositor);
    if (surface) {
        wl_surface_add_listener(surface, &probe_surface_listener, &probe);

        struct zwlr_layer_surface_v1 *layer_surface = zwlr_layer_shell_v1_get_layer_surface(
            state->layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "miru"
        );
        if (layer_surface) {
            wl_display_roundtrip(state->display);
            zwlr_layer_surface_v1_destroy(layer_surface);
        }
        wl_surface_destroy(surface);
    }

    struct miru_output *output;
    if (probe.entered) {
        wl_list_for_each(output, &state->outputs, link)
        {
            if (output->global_name == probe.entered_name) {
                MIRU_LOG("output: compositor picked %s", output_label(output));
                return output;
            }
        }
    }

    if (wl_list_empty(&state->outputs))
        return NULL;

    struct miru_output *fallback = wl_container_of(state->outputs.next, fallback, link);
    MIRU_LOG("output: no output reported by the compositor, falling back to %s", output_label(fallback));
    return fallback;
}

void wayland_state_cleanup(struct miru_state *state)
{
    struct miru_output *output;
    struct miru_output *tmp;
    wl_list_for_each_safe(output, tmp, &state->outputs, link)
    {
        output_destroy(output);
    }
    state->overlay_output = NULL;

    if (state->display) {
        wl_display_disconnect(state->display);
    }
}
