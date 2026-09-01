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
#define SPOTLIGHT_RADIUS_EPSILON 0.5f
#define SPOTLIGHT_DIM_EPSILON 0.01f

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

    int have_capture = ls->capture && ls->capture->buffer;
    uint32_t format = have_capture ? ls->capture->format : WL_SHM_FORMAT_ARGB8888;
    (void)format;

    ls->buffer_width = have_capture ? (int)ls->capture->width : ls->width;
    ls->buffer_height = have_capture ? (int)ls->capture->height : ls->height;

    if (!ls->configured) {
        ls->zoom = ls->zoom_default;
        ls->display_zoom = ls->zoom_default;

        /* Prefer position seeded in layer_surface_create (last pointer).
           Fall back to center only when we have nothing better. */
        if (!ls->cursor_seeded) {
            ls->cursor_x = ls->buffer_width / 2.0;
            ls->cursor_y = ls->buffer_height / 2.0;
            ls->display_cursor_x = ls->cursor_x;
            ls->display_cursor_y = ls->cursor_y;
        }
    }

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

    if (ls->compositor && ls->surface && ls->width > 0 && ls->height > 0) {
        struct wl_region *opaque = wl_compositor_create_region(ls->compositor);
        if (opaque) {
            wl_region_add(opaque, 0, 0, ls->width, ls->height);
            wl_surface_set_opaque_region(ls->surface, opaque);
            wl_region_destroy(opaque);
        }
    }

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

    ls->cursor_seeded = false;
    ls->cursor_snap_pending = true;

    ls->help_visible = false;

    annotation_state_init(&ls->annotations);

    if (config->has_initial_cursor) {
        ls->cursor_x = config->initial_cursor_x;
        ls->cursor_y = config->initial_cursor_y;
        ls->display_cursor_x = config->initial_cursor_x;
        ls->display_cursor_y = config->initial_cursor_y;
        ls->cursor_seeded = true;
        ls->cursor_snap_pending = false;
    }

    if (egl_init(&ls->egl, state->display) != 0) {
        fprintf(stderr, "failed to initialize EGL\n");
        return -1;
    }

    ls->compositor = state->compositor;
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
    if (ls->zoom > ls->zoom_max)
        ls->zoom = ls->zoom_max;
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

    if (fabsf(ls->display_spotlight_radius - target_r) > SPOTLIGHT_RADIUS_EPSILON)
        return true;
    if (fabsf(ls->display_spotlight_dim - target_d) > SPOTLIGHT_DIM_EPSILON)
        return true;

    return false;
}

void layer_surface_render(struct miru_layer_surface *ls)
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!ls->configured || !ls->egl.egl_window)
        return;

    bool animating = layer_surface_is_animating(ls);
    if (!animating && !ls->dirty)
        return;

    float t = ls->smooth_enabled ? (1.0f - expf(-SMOOTH_SPEED * (1.0f / 60.0f))) : 1.0f;
    ls->display_zoom += (ls->zoom - ls->display_zoom) * t;

    if (!ls->annotations.mode) {
        ls->display_cursor_x += (ls->cursor_x - ls->display_cursor_x) * t;
        ls->display_cursor_y += (ls->cursor_y - ls->display_cursor_y) * t;
    }

    float speed = ls->spotlight_animation_speed > 0.0f ? ls->spotlight_animation_speed : 14.0f;
    float st = 1.0f - expf(-speed * (1.0f / 60.0f));
    float target_radius = ls->spotlight_enabled ? ls->spotlight_radius : 0.0f;
    float target_dim = ls->spotlight_enabled ? ls->spotlight_dim : 0.0f;
    ls->display_spotlight_radius += (target_radius - ls->display_spotlight_radius) * st;
    ls->display_spotlight_dim += (target_dim - ls->display_spotlight_dim) * st;

    if (fabsf(ls->display_spotlight_radius - target_radius) <= SPOTLIGHT_RADIUS_EPSILON)
        ls->display_spotlight_radius = target_radius;
    if (fabsf(ls->display_spotlight_dim - target_dim) <= SPOTLIGHT_DIM_EPSILON)
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

    gl_renderer_draw_annotations(
        &ls->gl, &ls->annotations, src_left, src_top, src_w, src_h, ls->buffer_width, ls->buffer_height
    );

    if (ls->help_visible) {
        gl_renderer_draw_help(&ls->gl, ls->buffer_width, ls->buffer_height);
    }
    egl_swap_buffers(&ls->egl);
    ls->dirty = false;
}
