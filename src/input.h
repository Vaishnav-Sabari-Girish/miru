#ifndef INPUT_H
#define INPUT_H

#include <signal.h>
#include "wayland_state.h"
#include "layer_surface.h"

struct miru_input_ctx {
    struct miru_layer_surface *ls;
    volatile sig_atomic_t *request_deactivate;
};

void input_setup(struct miru_state *state, struct miru_input_ctx *ctx);

#endif // !INPUT_H
