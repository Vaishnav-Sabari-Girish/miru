#ifndef POINTER_TRACK_H
#define POINTER_TRACK_H

#include <stdbool.h>
#include "wayland_state.h"

struct miru_pointer_track {
    struct ext_image_copy_capture_cursor_session_v1 *session;
    bool has;
    double x, y;
};

bool pointer_track_start(struct miru_pointer_track *t, const struct miru_state *state, struct wl_output *output);

void pointer_track_stop(struct miru_pointer_track *t);

#endif // !POINTER_TRACK_H
