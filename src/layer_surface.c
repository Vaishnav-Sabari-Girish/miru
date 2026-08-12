#include "egl_context.h"
#include "gl_renderer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
/* #include <string.h> */
/* #include <stdint.h> */
#include <math.h>
#include <wayland-client-protocol.h>
#include "layer_surface.h"
/* #include "shm_buffer.h" */
/* #include <sys/types.h> */

#define SMOOTH_SPEED 12.0f
#define ZOOM_EPSILON 0.01f
#define CURSOR_EPSILON 0.5

/* static void handle_buffer_release(void *data, struct wl_buffer *buffer) */
/* { */
/*     (void)buffer; */
/*     // data is the specific slot this buffer belongs to, not the whole */
/*     // layer_surface — each slot tracks its own busy state independently */
/*     struct miru_buffer_slot *slot = data; */
/*     slot->busy = 0; */
/* } */

/* static const struct wl_buffer_listener buffer_listener = { */
/*     .release = handle_buffer_release, */
/* }; */

/* static void free_slot(struct miru_buffer_slot *slot) */
/* { */
/*     shm_buffer_free(slot->shm_data, slot->shm_size); */
/*     if (slot->buffer) { */
/*         wl_buffer_destroy(slot->buffer); */
/*     } */
/*     *slot = (struct miru_buffer_slot){ 0 }; */
/* } */

/* static int alloc_slot(struct miru_layer_surface *ls, struct miru_buffer_slot *slot, uint32_t format) */
/* { */
/*     void *pixels = NULL; */
/*     size_t size = 0; */
/*     slot->buffer = shm_buffer_create(ls->shm, ls->buffer_width, ls->buffer_height, format, &pixels, &size); */
/*     if (!slot->buffer) { */
/*         return -1; */
/*     } */
/*     slot->shm_data = pixels; */
/*     slot->shm_size = size; */
/*     slot->busy = 0; */
/*     wl_buffer_add_listener(slot->buffer, &buffer_listener, slot); */
/*     return 0; */

/* static void blit_and_commit(struct miru_layer_surface *ls) */
/* { */
/*     // pick whichever slot isn't currently attached/awaiting release from the */
/*     // compositor. with two slots this should basically always find one — */
/*     // if somehow neither is free, skip this render rather than tear a buffer */
/*     // the compositor might still be reading */
/*     struct miru_buffer_slot *slot = NULL; */
/*     if (!ls->slots[0].busy) { */
/*         slot = &ls->slots[0]; */
/*     } else if (!ls->slots[1].busy) { */
/*         slot = &ls->slots[1]; */
/*     } else { */
/*         return; */
/*     } */

/*     int have_capture = ls->capture && ls->capture->buffer; */

/*     if (have_capture) { */
/*         int bw = ls->buffer_width; */
/*         int bh = ls->buffer_height; */
/*         float z = ls->zoom < 1.0f ? 1.0f : ls->zoom; // never zoom below 1:1 */

/*         float src_w = (float)bw / z; */
/*         float src_h = (float)bh / z; */

/*         float src_left = (float)ls->cursor_x - src_w / 2.0f; */
/*         float src_top = (float)ls->cursor_y - src_h / 2.0f; */

/*         if (src_left < 0) */
/*             src_left = 0; */
/*         if (src_top < 0) */
/*             src_top = 0; */
/*         if (src_left + src_w > bw) */
/*             src_left = (float)bw - src_w; */
/*         if (src_top + src_h > bh) */
/*             src_top = (float)bh - src_h; */

/*         int dst_stride = bw * 4; */
/*         int src_stride = (int)ls->capture->stride; */

/*         float dst_cursor_x = (float)ls->cursor_x; */
/*         float dst_cursor_y = (float)ls->cursor_y; */

/*         float inner = ls->spotlight_radius - ls->spotlight_softness; */
/*         float outer = ls->spotlight_radius + ls->spotlight_softness; */
/*         if (inner < 0.0f) */
/*             inner = 0.0f; */

/*         float inner_sq = inner * inner; */
/*         float outer_sq = outer * outer; */
/*         float inv_softness = (outer > inner) ? (1.0f / (outer - inner)) : 0.0f; */

/*         float dim_factor = 1.0f - ls->spotlight_dim; */
/*         if (dim_factor < 0.0f) */
/*             dim_factor = 0.0f; */

/*         float spotlight_dim = ls->spotlight_dim; */
/*         int spotlight_enabled = ls->spotlight_enabled; */

/*         uint8_t *dst = (uint8_t *)slot->shm_data; */
/*         const uint8_t *src = (const uint8_t *)ls->capture->shm_data; */

/*         for (int dy = 0; dy < bh; dy++) { */
/*             int sy = (int)(src_top + (float)dy / z); */
/*             if (sy < 0) */
/*                 sy = 0; */
/*             if (sy >= bh) */
/*                 sy = bh - 1; */
/*             int real_sy = ls->capture->y_invert ? (bh - 1 - sy) : sy; */
/*             const uint8_t *src_row = src + (size_t)real_sy * src_stride; */
/*             uint8_t *dst_row = dst + (size_t)dy * dst_stride; */

/*             float ddy = (float)dy - dst_cursor_y; */
/*             float ddy_sq = ddy * ddy; */

/*             int row_fully_outside = spotlight_enabled && (ddy_sq > outer_sq); */

/*             for (int dx = 0; dx < bw; dx++) { */
/*                 int sx = (int)(src_left + (float)dx / z); */
/*                 if (sx < 0) */
/*                     sx = 0; */
/*                 if (sx >= bw) */
/*                     sx = bw - 1; */
/*                 memcpy(dst_row + (size_t)dx * 4, src_row + (size_t)sx * 4, 4); */

/*                 if (row_fully_outside) { */
/*                     uint8_t *px = dst_row + (size_t)dx * 4; */
/*                     px[0] = (uint8_t)((float)px[0] * dim_factor); */
/*                     px[1] = (uint8_t)((float)px[1] * dim_factor); */
/*                     px[2] = (uint8_t)((float)px[2] * dim_factor); */
/*                 } else if (spotlight_enabled) { */
/*                     float ddx = (float)dx - dst_cursor_x; */
/*                     float dst_sq = ddx * ddx + ddy_sq; */

/*                     float brightness; */
/*                     if (dst_sq <= inner_sq) { */
/*                         brightness = 1.0f; // inside the spotlight, full brightness */
/*                     } else if (dst_sq >= outer_sq) { */
/*                         brightness = dim_factor; // fully dimmed */
/*                     } else { */
/*                         float dist = sqrtf(dst_sq); */
/*                         float t = (dist - inner) * inv_softness; */
/*                         t = t * t * (3.0f - 2.0f * t); */
/*                         brightness = 1.0f - spotlight_dim * t; */
/*                         if (brightness < 0.0f) */
/*                             brightness = 0.0f; */
/*                     } */

/*                     uint8_t *px = dst_row + (size_t)dx * 4; */
/*                     px[0] = (uint8_t)((float)px[0] * brightness); */
/*                     px[1] = (uint8_t)((float)px[1] * brightness); */
/*                     px[2] = (uint8_t)((float)px[2] * brightness); */
/*                 } */
/*             } */
/*         } */

/*         wl_surface_set_buffer_scale(ls->surface, ls->output_scale); */
/*     } else { */
/*         wl_surface_set_buffer_scale(ls->surface, 1); */
/*         uint32_t *pixel_data = (uint32_t *)slot->shm_data; */
/*         uint32_t color = 0x88202020; */
/*         for (int i = 0; i < ls->buffer_width * ls->buffer_height; i++) { */
/*             pixel_data[i] = color; */
/*         } */
/*     } */

/*     wl_surface_attach(ls->surface, slot->buffer, 0, 0); */
/*     wl_surface_damage_buffer(ls->surface, 0, 0, ls->buffer_width, ls->buffer_height); */
/*     wl_surface_commit(ls->surface); */

/*     slot->busy = 1; */

static void
handle_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height)
{
    struct miru_layer_surface *ls = data;

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    if (width == 0 || height == 0) {
        fprintf(stderr, "ignoring 0x0 configure, waiting for a real size\n");
        return;
    }

    ls->width = (int)width;
    ls->height = (int)height;

    /* // free both slots' previous buffer/mapping first, or repeated configures */
    /* // (resize, output move) leak a wl_buffer + mmap region every time */
    /* free_slot(&ls->slots[0]); */
    /* free_slot(&ls->slots[1]); */

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
    /* int buffer_width = have_capture ? (int)ls->capture->width : ls->width; */
    /* int buffer_height = have_capture ? (int)ls->capture->height : ls->height; */
    /* ls->buffer_width = buffer_width; */
    /* ls->buffer_height = buffer_height; */
    ls->buffer_width = have_capture ? (int)ls->capture->width : ls->width;
    ls->buffer_height = have_capture ? (int)ls->capture->height : ls->height;

    if (!ls->configured) {
        ls->zoom = ls->zoom_default;
        ls->display_zoom = ls->zoom_default;
        ls->cursor_x = ls->buffer_width / 2.0;
        ls->cursor_y = ls->buffer_height / 2.0;
        ls->display_cursor_x = ls->cursor_x;
        ls->display_cursor_y = ls->cursor_y;
    }

    /* if (alloc_slot(ls, &ls->slots[0], format) != 0 || alloc_slot(ls, &ls->slots[1], format) != 0) { */
    /*     fprintf(stderr, "failed to create shm buffers\n"); */
    /*     free_slot(&ls->slots[0]); */
    /*     free_slot(&ls->slots[1]); */
    /*     ls->configured = false; */
    /*     return; */
    //}
    wl_surface_set_buffer_scale(ls->surface, ls->output_scale);

    if (!ls->egl.egl_window) {
        if (egl_create_surface(&ls->egl, ls->surface, ls->buffer_width, ls->buffer_height) != 0) {
            fprintf(stderr, "failed to create EGL surface\n");
            return;
        }
        if (gl_renderer_init(&ls->gl) != 0) {
            fprintf(stderr, "failed to init GL renderer\n");
            return;
        }
    } else {
        egl_resize_surface(&ls->egl, ls->buffer_width, ls->buffer_height);
    }

    if (have_capture) {
        gl_renderer_upload_texture(
            &ls->gl,
            (const uint8_t *)ls->capture->shm_data,
            (int)ls->capture->width,
            (int)ls->capture->height,
            (int)ls->capture->stride
        );
    }

    ls->configured = true;
    ls->dirty = true;
    layer_surface_render(ls);
}

static void handle_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    (void)surface;
    struct miru_layer_surface *ls = data;
    fprintf(stderr, "compositor closed our layer surface\n");
    ls->configured = false;
    ls->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = handle_configure,
    .closed = handle_closed,
};

int layer_surface_create(
    struct miru_state *state,
    struct miru_layer_surface *ls,
    const struct miru_capture *capture,
    // float zoom_default,
    // float zoom_max
    const struct layer_surface_config *config
)
{
    ls->capture = capture;
    ls->output_scale = state->output_scale;
    ls->zoom_default = config->zoom_default;
    ls->zoom_max = config->zoom_max;
    ls->spotlight_radius = config->spotlight_radius;
    ls->spotlight_dim = config->spotlight_dim;
    ls->spotlight_softness = config->spotlight_softness;
    ls->smooth_enabled = config->smooth_enabled;

    if (egl_init(&ls->egl, state->display) != 0) {
        fprintf(stderr, "failed to initialize EGL\n");
        return -1;
    }

    ls->surface = wl_compositor_create_surface(state->compositor);
    if (!ls->surface)
        return -1;

    ls->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, ls->surface, state->output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "miru"
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
        ls->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
    );

    zwlr_layer_surface_v1_add_listener(ls->layer_surface, &layer_surface_listener, ls);
    wl_surface_commit(ls->surface);

    return 0;
}

void layer_surface_apply_config(struct miru_layer_surface *ls, const struct layer_surface_config *config)
{
    ls->zoom_max = config->zoom_max;
    ls->spotlight_radius = config->spotlight_radius;
    ls->spotlight_dim = config->spotlight_dim;
    ls->spotlight_softness = config->spotlight_softness;
    ls->smooth_enabled = config->smooth_enabled;

    if (ls->zoom > ls->zoom_max) {
        ls->zoom = ls->zoom_max;
    }

    ls->dirty = true;
}

void layer_surface_destroy(struct miru_layer_surface *ls)
{
    gl_renderer_cleanup(&ls->gl);
    egl_cleanup(&ls->egl);
    if (ls->layer_surface)
        zwlr_layer_surface_v1_destroy(ls->layer_surface);
    if (ls->surface)
        wl_surface_destroy(ls->surface);
}

bool layer_surface_is_animating(const struct miru_layer_surface *ls)
{
    /* free_slot(&ls->slots[0]); */
    /* free_slot(&ls->slots[1]); */
    /* if (ls->layer_surface) { */
    /*     zwlr_layer_surface_v1_destroy(ls->layer_surface); */
    /* } */
    /* if (ls->surface) { */
    /*     wl_surface_destroy(ls->surface); */
    /* } */
    if (!ls->configured)
        return false;
    if (fabsf(ls->display_zoom - ls->zoom) > ZOOM_EPSILON)
        return true;
    if (fabs(ls->display_cursor_x - ls->cursor_x) > CURSOR_EPSILON)
        return true;
    if (fabs(ls->display_cursor_y - ls->cursor_y) > CURSOR_EPSILON)
        return true;

    return false;
}

void layer_surface_render(struct miru_layer_surface *ls)
{
    if (!ls->configured || !ls->egl.egl_window) {
        return; // nothing allocated yet
    }

    bool animating = layer_surface_is_animating(ls);
    if (!animating && !ls->dirty)
        return; // nothing to do

    float t = ls->smooth_enabled ? (1.0f - expf(-SMOOTH_SPEED * (1.0f / 60.0f))) : 1.0f;
    ls->display_zoom += (ls->zoom - ls->display_zoom) * t;
    ls->display_cursor_x += (ls->cursor_x - ls->display_cursor_x) * t;
    ls->display_cursor_y += (ls->cursor_y - ls->display_cursor_y) * t;

    if (fabsf(ls->display_zoom - ls->zoom) <= ZOOM_EPSILON)
        ls->display_zoom = ls->zoom;
    if (fabs(ls->display_cursor_x - ls->cursor_x) <= CURSOR_EPSILON)
        ls->display_cursor_x = ls->cursor_x;

    if (fabs(ls->display_cursor_y - ls->cursor_y) <= CURSOR_EPSILON)
        ls->display_cursor_y = ls->cursor_y;

    float z = ls->display_zoom < 1.0f ? 1.0f : ls->display_zoom;
    float src_w = (float)ls->buffer_width / z;
    float src_h = (float)ls->buffer_height / z;

    float src_left = (float)ls->display_cursor_x - src_w / 2.0f;
    float src_top = (float)ls->display_cursor_y - src_h / 2.0f;

    if (src_left < 0)
        src_left = 0;
    if (src_top < 0)
        src_top = 0;
    if (src_left + src_w > ls->buffer_width)
        src_left = ls->buffer_width - src_w;
    if (src_top + src_h > ls->buffer_height)
        src_top = ls->buffer_height - src_h;

    int y_invert = ls->capture ? ls->capture->y_invert : 0;

    // spotlight cursor must be in on-screen (post-zoom) pixel space not
    // raw sourcae-buffer space
    float dst_cursor_x = ((float)ls->cursor_x - src_left) * z;
    float dst_cursor_y = ((float)ls->cursor_y - src_top) * z;

    gl_renderer_draw(
        &ls->gl,
        src_left / (float)ls->buffer_width,
        src_top / (float)ls->buffer_height,
        src_w / (float)ls->buffer_width,
        src_h / (float)ls->buffer_height,
        y_invert,
        dst_cursor_x,
        dst_cursor_y,
        ls->buffer_width,
        ls->buffer_height,
        ls->spotlight_enabled,
        ls->spotlight_radius,
        ls->spotlight_softness,
        ls->spotlight_dim
    );

    egl_swap_buffers(&ls->egl);
    ls->dirty = false;

    /* if (!ls->capture || !ls->capture->buffer) { */
    /*     return; // no fresh frame to render */
    /* } */

    /* // output resolution changing mid-session isn't handled */
    /* // would need a full buffer re-allocation, same as handle_configure does. */
    /* // for now, just drop the frame rather than write past the buffer's actual size */
    /* if ((int)ls->capture->width != ls->buffer_width || (int)ls->capture->height != ls->buffer_height) { */
    /*     fprintf( */
    /*         stderr, */
    /*         "layer_surface_render: capture size changed (%dx%d vs %dx%d), dropping frame\n", */
    /*         ls->capture->width, */
    /*         ls->capture->height, */
    /*         ls->buffer_width, */
    /*         ls->buffer_height */
    /*     ); */
    /*     return; */
    /* } */

    /* blit_and_commit(ls); */
}
