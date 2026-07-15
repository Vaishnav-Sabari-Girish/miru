#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "layer_surface.h"
#include "shm_buffer.h"
#include <sys/types.h>

static void
handle_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height)
{
    struct miru_layer_surface *ls = data;

    zwlr_layer_surface_v1_ack_configure(surface, serial); // must ack every configure regardless of size

    // layer-shell spec: 0x0 means "client decides", not "render nothing" —
    // we don't track real output dimensions yet, so just wait for a real one
    if (width == 0 || height == 0) {
        fprintf(stderr, "ignoring 0x0 configure, waiting for a real size\n");
        return;
    }

    ls->width = (int)width;
    ls->height = (int)height;

    // free the previous buffer/mapping first, or repeated configures
    // (resize, output move) leak a wl_buffer + mmap region every time
    if (ls->buffer) {
        wl_buffer_destroy(ls->buffer);
        ls->buffer = NULL;
    }
    if (ls->shm_data) {
        shm_buffer_free(ls->shm_data, ls->shm_size);
        ls->shm_data = NULL;
        ls->shm_size = 0;
    }

    int have_capture = ls->capture && ls->capture->buffer;

    // use the captured frame's own pixel format, so an XRGB8888 capture
    // (no meaningful alpha byte) isn't misread as ARGB8888 and rendered
    // translucent by the compositor's alpha blending
    uint32_t format = have_capture ? ls->capture->format : WL_SHM_FORMAT_ARGB8888;

    // the capture is in physical pixels, but ls->width/height (from the
    // configure event) are logical pixels on scaled outputs — allocate the
    // buffer at the capture's real size and tell the compositor the scale
    // via wl_surface_set_buffer_scale, instead of squeezing physical pixels
    // into a logical-sized buffer (which is what was making it look zoomed)
    int buffer_width = have_capture ? (int)ls->capture->width : ls->width;
    int buffer_height = have_capture ? (int)ls->capture->height : ls->height;

    void *pixels = NULL;
    size_t size = 0;
    ls->buffer = shm_buffer_create(ls->shm, buffer_width, buffer_height, format, &pixels, &size);
    if (!ls->buffer) {
        fprintf(stderr, "failed to create shm buffer\n");
        return;
    }
    ls->shm_data = pixels;
    ls->shm_size = size;

    /*uint32_t *pixel_data = (uint32_t *)pixels;
    uint32_t color = 0x88202020;
    for (int i = 0; i < ls->width * ls->height; i++) {
        pixel_data[i] = color;
    }*/
    if (have_capture) {
        // blit the captured frame into the buffer row by row
        // the 2 buffers can have different strides even when dimensions match
        size_t row_bytes = (size_t)buffer_width * 4;
        int dst_stride = buffer_width * 4;
        int src_stride = (int)ls->capture->stride;

        uint8_t *dst = (uint8_t *)pixels;
        const uint8_t *src = (const uint8_t *)ls->capture->shm_data;

        for (int y = 0; y < buffer_height; y++) {
            // y_invert means that the compositor handed the frame upside-down
            // read source rows bottom-to-top while writing top-to-bottom
            int src_y = ls->capture->y_invert ? ((int)ls->capture->height - 1 - y) : y;
            memcpy(dst + (size_t)y * dst_stride, src + (size_t)src_y * src_stride, row_bytes);
        }
        wl_surface_set_buffer_scale(ls->surface, ls->output_scale);
    } else {
        // no capture available (failed earlier). Fall back to flat test color
        wl_surface_set_buffer_scale(ls->surface, 1);
        uint32_t *pixel_data = (uint32_t *)pixels;
        uint32_t color = 0x88202020;
        for (int i = 0; i < ls->width * ls->height; i++) {
            pixel_data[i] = color;
        }
    }

    wl_surface_attach(ls->surface, ls->buffer, 0, 0);
    wl_surface_damage_buffer(ls->surface, 0, 0, buffer_width, buffer_height);
    wl_surface_commit(ls->surface);

    ls->configured = 1;
}

static void handle_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    (void)surface;
    struct miru_layer_surface *ls = data;
    fprintf(stderr, "compositor closed our layer surface\n");
    ls->configured = 0;
    ls->closed = 1; // main's loop checks this and exits instead of spinning on a dead surface
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = handle_configure,
    .closed = handle_closed,
};

int layer_surface_create(struct miru_state *state, struct miru_layer_surface *ls, const struct miru_capture *capture)
{
    ls->shm = state->shm;
    ls->capture = capture;
    ls->output_scale = state->output_scale;

    ls->surface = wl_compositor_create_surface(state->compositor);
    if (!ls->surface) {
        return -1;
    }

    ls->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, ls->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "miru"
    );
    if (!ls->layer_surface) {
        wl_surface_destroy(ls->surface);
        return -1;
    }

    zwlr_layer_surface_v1_set_anchor(
        ls->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
    );
    zwlr_layer_surface_v1_set_exclusive_zone(ls->layer_surface, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        ls->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE
    );

    zwlr_layer_surface_v1_add_listener(ls->layer_surface, &layer_surface_listener, ls);
    wl_surface_commit(ls->surface);

    return 0;
}

void layer_surface_destroy(struct miru_layer_surface *ls)
{
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
