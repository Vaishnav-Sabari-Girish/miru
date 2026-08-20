#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <linux/input-event-codes.h>
#include "input.h"
#include "debug.h"
#include "layer_surface.h"
#include <wayland-client-protocol.h>

#define ZOOM_MIN 1.0f

#define PAN_STEP 50.0

#define RADIUS_MIN 10.0f
#define RADIUS_MAX 2000.0f

bool miru_debug_enabled(void)
{
    static int cached = -1;
    if (cached == -1) {
        const char *v = getenv("MIRU_DEBUG");
        cached = (v && *v && strcmp(v, "0") != 0) ? 1 : 0;
    }

    return cached != 0;
}

static void clamp_radius(struct miru_layer_surface *ls)
{
    if (ls->spotlight_radius < RADIUS_MIN)
        ls->spotlight_radius = RADIUS_MIN;

    if (ls->spotlight_radius > RADIUS_MAX)
        ls->spotlight_radius = RADIUS_MAX;
}

static void apply_cursor_visibility(struct miru_input_ctx *ctx)
{
    if (!ctx->pointer || !ctx->pointer_enter_serial)
        return;

    if (!ctx->show_cursor) {
        wl_pointer_set_cursor(ctx->pointer, ctx->pointer_enter_serial, NULL, 0, 0);
    }
}

static void adjust_spotlight_radius(struct miru_layer_surface *ls, float delta)
{
    ls->spotlight_radius += delta;
    clamp_radius(ls);

    if (miru_debug_enabled()) {
        fprintf(stderr, "spotlight radius -> %.1f\n", ls->spotlight_radius);
    }

    ls->dirty = true;
}

static double pan_step(struct miru_layer_surface *ls)
{
    return PAN_STEP / ls->zoom;
}

static void clamp_zoom(struct miru_layer_surface *ls)
{
    if (ls->zoom < ZOOM_MIN)
        ls->zoom = ZOOM_MIN;
    if (ls->zoom > ls->zoom_max) {
        if (miru_debug_enabled()) {
            fprintf(stderr, "clamp_zoom: clamping %.3f down to zoom_max = %.3f\n", ls->zoom, ls->zoom_max);
        }
        ls->zoom = ls->zoom_max;
    }
}

static void clamp_pan(struct miru_layer_surface *ls)
{
    double half_view_width = (double)ls->buffer_width / (2.0 * ls->zoom);
    double half_view_height = (double)ls->buffer_height / (2.0 * ls->zoom);
    if (ls->cursor_x < half_view_width)
        ls->cursor_x = half_view_width;
    if (ls->cursor_x > ls->buffer_width - half_view_width)
        ls->cursor_x = ls->buffer_width - half_view_width;

    if (ls->cursor_y < half_view_height)
        ls->cursor_y = half_view_height;
    if (ls->cursor_y > ls->buffer_height - half_view_height)
        ls->cursor_y = ls->buffer_height - half_view_height;
}

static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
    // (void)data;
    (void)serial;
    (void)pointer;
    (void)surface;

    struct miru_input_ctx *ctx = data;
    ctx->has_pointer_enter = false;
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
    // (void)pointer;
    // (void)serial;
    (void)surface;

    struct miru_input_ctx *ctx = data;
    if (!ctx->ls->configured)
        return;

    ctx->pointer = pointer;
    ctx->pointer_enter_serial = serial;
    ctx->has_pointer_enter = true;
    apply_cursor_visibility(ctx);

    ctx->ls->cursor_x = wl_fixed_to_double(x) * ctx->ls->output_scale;
    ctx->ls->cursor_y = wl_fixed_to_double(y) * ctx->ls->output_scale;

    ctx->ls->dirty = true;
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
    ctx->ls->dirty = true;
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

    if (ctx->ctrl_held) {
        float delta = (v > 0) ? -ctx->radius_step : ctx->radius_step;
        adjust_spotlight_radius(ctx->ls, delta);
        return;
    }

    if (miru_debug_enabled()) {
        fprintf(
            stderr, "pointer_axis: v=%.3f zoom_increment=%.3f zoom_before=%.3f\n", v, ctx->zoom_increment, ctx->ls->zoom
        );
    }

    if (v > 0) {
        ctx->ls->zoom -= ctx->zoom_increment;
    } else {
        ctx->ls->zoom += ctx->zoom_increment;
    }

    clamp_zoom(ctx->ls);

    if (miru_debug_enabled()) {
        fprintf(stderr, "pointer_axis: zoom_after=%.3f\n", ctx->ls->zoom);
    }

    ctx->ls->dirty = true;
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
    (void)keyboard;
    (void)serial;
    (void)surface;

    struct miru_input_ctx *ctx = data;
    input_reset_repeat(ctx);
    ctx->shift_held = false;
    ctx->ctrl_held = false;
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
    (void)keyboard;

    struct miru_input_ctx *ctx = data;

    ctx->repeat_rate = rate;
    ctx->repeat_delay = delay;

    if (rate <= 0) {
        input_reset_repeat(ctx);
    }
}

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    (void)data;
    (void)keyboard;
    (void)format;
    (void)size;

    close(fd);
}

static int handle_key_action(struct miru_input_ctx *ctx, uint32_t key)
{
    if (miru_debug_enabled()) {
        fprintf(
            stderr,
            "handle_key_action: key=%u zoom_increment=%.3f zoom_before=%.3f zoom_max=%.3f\n",
            key,
            ctx->zoom_increment,
            ctx->ls->zoom,
            ctx->ls->zoom_max
        );
    }

    if (key == KEY_EQUAL || key == KEY_KPPLUS) {
        if (ctx->ctrl_held) {
            adjust_spotlight_radius(ctx->ls, ctx->radius_step);
            return 1;
        }

        ctx->ls->zoom += ctx->zoom_increment;
        clamp_zoom(ctx->ls);
    } else if (key == KEY_MINUS || key == KEY_KPMINUS) {
        if (ctx->ctrl_held) {
            adjust_spotlight_radius(ctx->ls, -ctx->radius_step);
            return 1;
        }

        ctx->ls->zoom -= ctx->zoom_increment;
        clamp_zoom(ctx->ls);
    } else if (key == KEY_LEFT || key == KEY_A) {
        ctx->ls->cursor_x -= pan_step(ctx->ls);
        clamp_pan(ctx->ls);
    } else if (key == KEY_RIGHT || key == KEY_D) {
        ctx->ls->cursor_x += pan_step(ctx->ls);
        clamp_pan(ctx->ls);
    } else if (key == KEY_UP || key == KEY_W) {
        ctx->ls->cursor_y -= pan_step(ctx->ls);
        clamp_pan(ctx->ls);
    } else if (key == KEY_DOWN || key == KEY_S) {
        ctx->ls->cursor_y += pan_step(ctx->ls);
        clamp_pan(ctx->ls);
    } else {
        return 0;
    }

    if (miru_debug_enabled()) {
        fprintf(stderr, "handle_key_action: zoom_after=%.3f\n", ctx->ls->zoom);
    }

    ctx->ls->dirty = true;
    return 1;
}

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static struct miru_repeat_slot *find_repeat_slot(struct miru_input_ctx *ctx, uint32_t key, int allow_alloc)
{
    struct miru_repeat_slot *free_slot = NULL;
    for (int i = 0; i < MIRU_MAX_REPEAT_KEYS; i++) {
        struct miru_repeat_slot *slot = &ctx->repeat_slots[i];
        if (slot->active && slot->key == key) {
            return slot;
        }
        if (!slot->active && !free_slot) {
            free_slot = slot;
        }
    }

    return allow_alloc ? free_slot : NULL;
}

void input_process_repeats(struct miru_input_ctx *ctx)
{
    long long t = now_ms();
    for (int i = 0; i < MIRU_MAX_REPEAT_KEYS; i++) {
        struct miru_repeat_slot *slot = &ctx->repeat_slots[i];
        if (!slot->active || t < slot->next_repeat_at) {
            continue;
        }
        handle_key_action(ctx, slot->key);

        slot->next_repeat_at =
            t + (ctx->repeat_rate > 0 ? (1000 / ctx->repeat_rate + (1000 % ctx->repeat_rate != 0)) : 1000);
    }
}

int input_next_repeat_timeout(struct miru_input_ctx *ctx)
{
    long long t = now_ms();

    long long soonest = -1;

    for (int i = 0; i < MIRU_MAX_REPEAT_KEYS; i++) {
        struct miru_repeat_slot *slot = &ctx->repeat_slots[i];
        if (!slot->active) {
            continue;
        }

        long long remaining = slot->next_repeat_at - t;

        if (remaining < 0) {
            remaining = 0;
        }
        if (soonest == -1 || remaining < soonest) {
            soonest = remaining;
        }
    }

    return (int)soonest;
}

void input_reset_repeat(struct miru_input_ctx *ctx)
{
    for (int i = 0; i < MIRU_MAX_REPEAT_KEYS; i++) {
        ctx->repeat_slots[i] = (struct miru_repeat_slot){ 0 };
    }
}

void input_set_show_cursor(struct miru_input_ctx *ctx, bool show)
{
    ctx->show_cursor = show;
    apply_cursor_visibility(ctx);
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

    if (key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT) {
        ctx->shift_held = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        return;
    }

    if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL) {
        ctx->ctrl_held = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        return;
    }
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        struct miru_repeat_slot *slot = find_repeat_slot(ctx, key, 0);
        if (slot) {
            *slot = (struct miru_repeat_slot){ 0 };
        }
        return;
    }

    if (key == KEY_ESC) {
        if (ctx->request_deactivate) {
            *ctx->request_deactivate = 1;
        }
        return;
    }

    if (key == KEY_TAB) {
        ctx->ls->spotlight_enabled = !ctx->ls->spotlight_enabled;
        ctx->ls->dirty = true;
        return;
    }

    if (handle_key_action(ctx, key) && ctx->repeat_rate > 0) {
        struct miru_repeat_slot *slot = find_repeat_slot(ctx, key, 1);
        if (slot) {
            slot->key = key;
            slot->active = true;
            slot->next_repeat_at = now_ms() + ctx->repeat_delay;
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
