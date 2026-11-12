#include <string.h>
#include "pointer_track.h"
#include "debug.h"
#include <stdio.h>

static void session_enter(void *data, struct ext_image_copy_capture_cursor_session_v1 *session)
{
    (void)data;
    (void)session;
}

static void session_leave(void *data, struct ext_image_copy_capture_cursor_session_v1 *session)
{
    (void)data;
    (void)session;
}

static void session_position(void *data, struct ext_image_copy_capture_cursor_session_v1 *session, int32_t x, int32_t y)
{
    (void)session;
    struct miru_pointer_track *t = data;
    t->x = x;
    t->y = y;
    t->has = true;
}

static void session_hotspot(void *data, struct ext_image_copy_capture_cursor_session_v1 *session, int32_t x, int32_t y)
{
    (void)data;
    (void)session;
    (void)x;
    (void)y;
}

static const struct ext_image_copy_capture_cursor_session_v1_listener session_listener = {
    .enter = session_enter,
    .leave = session_leave,
    .position = session_position,
    .hotspot = session_hotspot,
};

bool pointer_track_start(struct miru_pointer_track *t, const struct miru_state *state, struct wl_output *output)
{
    memset(t, 0, sizeof(*t));

    if (!state->capture_source_manager || !state->copy_capture_manager) {
        MIRU_LOG("pointer: the compositor has not ext-image-copy-capture-v1 cursor sessions");
        return false;
    }

    if (!state->pointer || !output) {
        MIRU_LOG("pointer: no pointer or input to track");
        return false;
    }

    struct ext_image_capture_source_v1 *source =
        ext_output_image_capture_source_manager_v1_create_source(state->capture_source_manager, output);
    if (!source)
        return false;

    t->session = ext_image_copy_capture_manager_v1_create_pointer_cursor_session(
        state->copy_capture_manager, source, state->pointer
    );

    ext_image_capture_source_v1_destroy(source);

    if (!t->session)
        return false;

    ext_image_copy_capture_cursor_session_v1_add_listener(t->session, &session_listener, t);
    return true;
}

void pointer_track_stop(struct miru_pointer_track *t)
{
    if (t->session) {
        ext_image_copy_capture_cursor_session_v1_destroy(t->session);
        t->session = NULL;
    }

    t->has = false;
}
