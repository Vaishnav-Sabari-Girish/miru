#include <stdio.h>
#include "layer_surface.h"
#include "shm_buffer.h"

// called when the compositor tells us what size our surface actually got
static void handle_configure(void *data, struct zwlr_layer_surface_v1 *surface,
                              uint32_t serial, uint32_t width, uint32_t height) {
    struct miru_layer_surface *ls = data;

    ls->width = (int)width;
    ls->height = (int)height;

    zwlr_layer_surface_v1_ack_configure(surface, serial);          // must ack every configure event

    void *pixels = NULL;
    size_t size = 0;
    ls->buffer = shm_buffer_create(ls->shm, ls->width, ls->height, WL_SHM_FORMAT_ARGB8888, &pixels, &size);
    if (!ls->buffer) {
        fprintf(stderr, "failed to create shm buffer\n");
        return;
    }
    ls->shm_data = pixels;
    ls->shm_size = size;

    // fill every pixel with a translucent dark grey, just to prove the surface is actually visible
    // ARGB8888 premultiplied: alpha in the top byte, then red, green, blue
    uint32_t *pixel_data = (uint32_t *)pixels;
    uint32_t color = 0x88202020;                                   // ~53% alpha, dark grey
    for (int i = 0; i < ls->width * ls->height; i++) {
        pixel_data[i] = color;
    }

    wl_surface_attach(ls->surface, ls->buffer, 0, 0);              // hand the buffer to the surface
    wl_surface_damage_buffer(ls->surface, 0, 0, ls->width, ls->height); // mark it all dirty
    wl_surface_commit(ls->surface);                                // actually show it

    ls->configured = 1;
}

// called if the compositor wants us to close (e.g. the output we're on gets unplugged)
static void handle_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
    (void)surface;
    struct miru_layer_surface *ls = data;
    fprintf(stderr, "compositor closed our layer surface\n");
    ls->configured = 0;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = handle_configure,
    .closed = handle_closed,
};

int layer_surface_create(struct miru_state *state, struct miru_layer_surface *ls) {
    ls->shm = state->shm;                                          // needed later, inside handle_configure

    ls->surface = wl_compositor_create_surface(state->compositor);
    if (!ls->surface) {
        return -1;
    }

    // NULL output lets the compositor pick which monitor we show on, for now
    // "overlay" layer draws above every normal window and panel
    ls->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, ls->surface, NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "miru");
    if (!ls->layer_surface) {
        wl_surface_destroy(ls->surface);
        return -1;
    }

    // anchor all four edges + implicit size(0,0) below means "fill the whole output"
    zwlr_layer_surface_v1_set_anchor(ls->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    // -1 tells other layer-shell surfaces (bars etc.) not to reserve space around us
    zwlr_layer_surface_v1_set_exclusive_zone(ls->layer_surface, -1);

    // NONE for now, this is just a visual test, no input handling yet
    zwlr_layer_surface_v1_set_keyboard_interactivity(ls->layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    zwlr_layer_surface_v1_add_listener(ls->layer_surface, &layer_surface_listener, ls);

    // commit with no buffer yet, this is what triggers the compositor's first configure event
    wl_surface_commit(ls->surface);

    return 0;
}

void layer_surface_destroy(struct miru_layer_surface *ls) {
    shm_buffer_free(ls->shm_data, ls->shm_size);
    if (ls->buffer) {
        wl_buffer_destroy(ls->buffer);
    }
    if (ls->layer_surface) {
        zwlr_layer_surface_v1_destroy(ls->layer_surface);
    }
    if (ls->surface) {
        wl_surface_destroy(ls->surface);
    }
}
