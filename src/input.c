#include <unistd.h>
#include <linux/input-event-codes.h>
#include "input.h"
#include "layer_surface.h"
#include <wayland-client-protocol.h>

#define ZOOM_STEP 0.25f
#define ZOOM_MIN 1.0f
#define ZOOM_MAX 10.0f

static void clamp_zoom(struct miru_layer_surface *ls)
{
    if (ls->zoom < ZOOM_MIN)
        ls->zoom = ZOOM_MIN;
    if (ls->zoom > ZOOM_MAX)
        ls->zoom = ZOOM_MAX;
}

static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
    (void)data;
    (void)serial;
    (void)pointer;
    (void)surface;
}

static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    (void)data;
    (void)pointer;
    (void)serial;
    (void)time;
    (void)button;
    (void)state;
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
    (void)data;
    (void)pointer;
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source)
{
    (void)data;
    (void)pointer;
    (void)axis_source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis)
{
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete)
{
    (void)data;
    (void)pointer;
    (void)axis;
    (void)discrete;
}

static void pointer_enter(
    void *data,
    struct wl_pointer *pointer,
    uint32_t serial,
    struct wl_surface *surface,
    wl_fixed_t x,
    wl_fixed_t y
)
{
    (void)pointer;
    (void)serial;
    (void)surface;

    struct miru_input_ctx *ctx = data;
    if (!ctx->ls->configured)
        return;

    ctx->ls->cursor_x = wl_fixed_to_double(x) * ctx->ls->output_scale;
    ctx->ls->cursor_y = wl_fixed_to_double(y) * ctx->ls->output_scale;

    ctx->ls->dirty = 1;
}

static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    (void)pointer;
    (void)time;
    struct miru_input_ctx *ctx = data;
    if (!ctx->ls->configured)
        return;

    ctx->ls->cursor_x = wl_fixed_to_double(x) * ctx->ls->output_scale;
    ctx->ls->cursor_y = wl_fixed_to_double(y) * ctx->ls->output_scale;
    ctx->ls->dirty = 1;
}

static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    (void)pointer;
    (void)time;

    struct miru_input_ctx *ctx = data;
    if (!ctx->ls->configured)
        return;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
        return;

    double v = wl_fixed_to_double(value);
    if (v > 0) {
        ctx->ls->zoom -= ZOOM_STEP;
    } else {
        ctx->ls->zoom += ZOOM_STEP;
    }

    clamp_zoom(ctx->ls);
    ctx->ls->dirty = 1;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

static void keyboard_enter(
    void *data,
    struct wl_keyboard *keyboard,
    uint32_t serial,
    struct wl_surface *surface,
    struct wl_array *keys
)
{
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
}

static void keyboard_modifiers(
    void *data,
    struct wl_keyboard *keyboard,
    uint32_t serial,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group
)
{
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)mods_depressed;
    (void)mods_latched;
    (void)mods_locked;
    (void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
    (void)data;
    (void)keyboard;
    (void)rate;
    (void)delay;
}

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    (void)data;
    (void)keyboard;
    (void)format;
    (void)size;

    close(fd);
}

static void
keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    (void)keyboard;
    (void)serial;
    (void)time;

    struct miru_input_ctx *ctx = data;
    if (!ctx->ls->configured)
        return;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;

    if (key == KEY_EQUAL) {
        ctx->ls->zoom += ZOOM_STEP;
        clamp_zoom(ctx->ls);
        ctx->ls->dirty = 1;
    } else if (key == KEY_MINUS) {
        ctx->ls->zoom -= ZOOM_STEP;
        clamp_zoom(ctx->ls);
        ctx->ls->dirty = 1;
    } else if (key == KEY_ESC) {
        if (ctx->request_deactivate) {
            *ctx->request_deactivate = 1;
        }
    }
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

void input_attach_pointer_listener(struct wl_pointer *pointer, void *ctx)
{
    if (pointer && ctx) {
        wl_pointer_add_listener(pointer, &pointer_listener, ctx);
    }
}

void input_attach_keyboard_listener(struct wl_keyboard *keyboard, void *ctx)
{
    if (keyboard && ctx) {
        wl_keyboard_add_listener(keyboard, &keyboard_listener, ctx);
    }
}

void input_setup(struct miru_state *state, struct miru_input_ctx *ctx)
{
    state->input_ctx = ctx;
    if (state->pointer) {
        wl_pointer_add_listener(state->pointer, &pointer_listener, ctx);
    }
    if (state->keyboard) {
        wl_keyboard_add_listener(state->keyboard, &keyboard_listener, ctx);
    }
}
