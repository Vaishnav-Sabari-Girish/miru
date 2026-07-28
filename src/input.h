#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <signal.h>
#include "wayland_state.h"
#include "layer_surface.h"

#define MIRU_MAX_REPEAT_KEYS 8

struct miru_repeat_slot {
    uint32_t key;
    int active;
    long long next_repeat_at;
};

struct miru_input_ctx {
    struct miru_layer_surface *ls;
    volatile sig_atomic_t *request_deactivate;

    float zoom_increment;

    int repeat_rate;
    int repeat_delay;
    struct miru_repeat_slot repeat_slots[MIRU_MAX_REPEAT_KEYS];
};

void input_setup(struct miru_state *state, struct miru_input_ctx *ctx);

void input_attach_pointer_listener(struct wl_pointer *pointer, void *ctx);
void input_attach_keyboard_listener(struct wl_keyboard *keyboard, void *ctx);

void input_process_repeats(struct miru_input_ctx *ctx);

int input_next_repeat_timeout(struct miru_input_ctx *ctx);

void input_reset_repeat(struct miru_input_ctx *ctx);

#endif // !INPUT_H
