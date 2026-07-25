#ifndef INPUT_H
#define INPUT_H

#include <signal.h>
#include "wayland_state.h"
#include "layer_surface.h"

struct miru_input_ctx {
    struct miru_layer_surface *ls;
    volatile sig_atomic_t *request_deactivate;

    uint32_t repeat_key;
    int repeat_rate;
    int repeat_delay;
    int repeating;
    int repeat_started;
};

void input_setup(struct miru_state *state, struct miru_input_ctx *ctx);

void input_attach_pointer_listener(struct wl_pointer *pointer, void *ctx);
void input_attach_keyboard_listener(struct wl_keyboard *keyboard, void *ctx);

void input_repeat(struct miru_input_ctx *ctx);

#endif // !INPUT_H
