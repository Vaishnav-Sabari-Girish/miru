#include "egl_context.h"
#include "gl_renderer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <wayland-client-protocol.h>
#include "layer_surface.h"

#define SMOOTH_SPEED 12.0f
#define ZOOM_EPSILON 0.01f
#define CURSOR_EPSILON 0.5
#define SPOTLIGHT_EPSILON 0.5f

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
            (int)ls->capture->stride,
            ls->capture->format
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
    ls->spotlight_enabled = false;
    ls->display_spotlight_radius = 0.0f;
    ls->display_spotlight_dim = 0.0f;
    ls->spotlight_animation_speed = config->spotlight_animation_speed > 0.0f ? config->spotlight_animation_speed :
                                                                               14.0f;
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
    if (config->spotlight_animation_speed > 0.0f)
        ls->spotlight_animation_speed = config->spotlight_animation_speed;
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

    float target_r = ls->spotlight_enabled ? ls->spotlight_radius : 0.0f;
    float target_d = ls->spotlight_enabled ? ls->spotlight_dim : 0.0f;

    if (fabsf(ls->display_spotlight_radius - target_r) > SPOTLIGHT_EPSILON)
        return true;
    if (fabsf(ls->display_spotlight_dim - target_d) > 0.01f)
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

    float speed = ls->spotlight_animation_speed > 0.0f ? ls->spotlight_animation_speed : 14.0f;
    float st = 1.0f - expf(-speed * (1.0f / 60.0f));
    float target_radius = ls->spotlight_enabled ? ls->spotlight_radius : 0.0f;
    float target_dim = ls->spotlight_enabled ? ls->spotlight_dim : 0.0f;
    ls->display_spotlight_radius += (target_radius - ls->display_spotlight_radius) * st;
    ls->display_spotlight_dim += (target_dim - ls->display_spotlight_dim) * st;

    if (fabsf(ls->display_spotlight_radius - target_radius) <= SPOTLIGHT_EPSILON)
        ls->display_spotlight_radius = target_radius;
    if (fabsf(ls->display_spotlight_dim - target_dim) <= 0.01f)
        ls->display_spotlight_dim = target_dim;

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
    float dst_cursor_x = (float)ls->cursor_x;
    float dst_cursor_y = (float)ls->cursor_y;

    dst_cursor_y = (float)ls->buffer_height - dst_cursor_y;

    bool spotlight_active = ls->display_spotlight_radius > 0.5f || ls->display_spotlight_dim > 0.001f;

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
        spotlight_active,
        ls->display_spotlight_radius,
        ls->spotlight_softness,
        ls->display_spotlight_dim
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
