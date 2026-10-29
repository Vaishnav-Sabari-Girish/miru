#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "capture.h"
#include "shm_buffer.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include <wayland-client-protocol.h>
#include "debug.h"

#define DRM_FORMAT_ARGB8888 0x34325241u // AR24
#define DRM_FORMAT_XRGB8888 0x34325258u // XR24
#define DRM_FORMAT_BGR888 0x34324742u // BG24
// #define DRM_FORMAT_ABGR8888 0x34324241u  // AB24
// #define DRM_FORMAT_XBGR8888 0x34324258u  // XB24

// tracks one in-flight capture across its async callback sequence
struct capture_ctx {
    struct miru_state *state;
    struct miru_capture *out;
    bool done;
    bool ok;
    bool needs_conversion;
    struct wl_buffer *raw_buffer;
    void *raw_shm_data;
    size_t raw_shm_size;
    uint32_t raw_stride;
    uint32_t raw_width;
    uint32_t raw_height;
};

static const char *shm_format_name(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        return "ARGB8888";
    case WL_SHM_FORMAT_XRGB8888:
        return "XRGB8888";
    case WL_SHM_FORMAT_ABGR8888:
        return "ABGR8888";
    case WL_SHM_FORMAT_XBGR8888:
        return "XBGR8888";

    case DRM_FORMAT_ARGB8888:
        return "ARGB8888 (DRM FourCC)";
    case DRM_FORMAT_XRGB8888:
        return "XRGB8888 (DRM FourCC)";
        // case DRM_FORMAT_ABGR8888:
        //     return "ABGR8888 (DRM FourCC)";
        // case DRM_FORMAT_XBGR8888:
        //     return "XBGR8888 (DRM FourCC)";

    case DRM_FORMAT_BGR888:
        return "BGR888 (DRM FourCC, 24bpp packed)";
    default:
        return "Unknown";
    }
}

static void free_raw(struct capture_ctx *ctx)
{
    if (ctx->raw_shm_data) {
        shm_buffer_free(ctx->raw_shm_data, ctx->raw_shm_size);
        ctx->raw_shm_data = NULL;
        ctx->raw_shm_size = 0;
    }

    if (ctx->raw_buffer) {
        wl_buffer_destroy(ctx->raw_buffer);
        ctx->raw_buffer = NULL;
    }
}

static void handle_buffer(
    void *data,
    struct zwlr_screencopy_frame_v1 *frame,
    uint32_t format,
    uint32_t width,
    uint32_t height,
    uint32_t stride
)
{
    struct capture_ctx *ctx = data;

    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XBGR8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        break;
    case DRM_FORMAT_BGR888:
        ctx->needs_conversion = true;
        break;

    default:
        // fprintf(stderr, "capture: unsupported pixel format %u (%s)\n", format, shm_format_name(format));
        MIRU_LOG("capture: unsupported pixel format %u (%s)", format, shm_format_name(format));
        ctx->done = true;
        ctx->ok = false;
        return;
    }

    if (ctx->needs_conversion) {
        void *raw_pixels = NULL;
        size_t raw_size = 0;
        ctx->raw_buffer = shm_buffer_create_stride_bpp(
            ctx->state->shm, (int)width, (int)height, (int)stride, 3, format, &raw_pixels, &raw_size
        );

        if (!ctx->raw_buffer) {
            // fprintf(stderr, "capture: failed to allocate raw 24bpp buffer for frame\n");
            MIRU_LOG("capture: failed to allocate raw 24bpp buffer for frame");
            ctx->done = true;
            ctx->ok = false;
            return;
        }

        ctx->raw_shm_data = raw_pixels;
        ctx->raw_shm_size = raw_size;
        ctx->raw_stride = stride;
        ctx->raw_width = width;
        ctx->raw_height = height;

        ctx->out->width = width;
        ctx->out->height = height;

        zwlr_screencopy_frame_v1_copy(frame, ctx->raw_buffer);
        return;
    }

    ctx->out->format = format;
    ctx->out->width = width;
    ctx->out->height = height;
    ctx->out->stride = stride;

    void *pixels = NULL;
    size_t size = 0;
    // must use the compositor's exact stride, it may include row padding we can't guess at
    ctx->out->buffer =
        shm_buffer_create_stride(ctx->state->shm, (int)width, (int)height, (int)stride, format, &pixels, &size);
    if (!ctx->out->buffer) {
        // fprintf(stderr, "capture: failed to allocate shm buffer for frame\n");
        MIRU_LOG("capture: failed to allocate shm buffer for frame");
        ctx->done = true;
        ctx->ok = false;
        return;
    }
    ctx->out->shm_data = pixels;
    ctx->out->shm_size = size;

    zwlr_screencopy_frame_v1_copy(frame, ctx->out->buffer);
}

static void handle_flags(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
    (void)frame;
    struct capture_ctx *ctx = data;
    ctx->out->y_invert = (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
}

static void handle_ready(
    void *data,
    struct zwlr_screencopy_frame_v1 *frame,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec
)
{
    (void)frame;
    (void)tv_sec_hi;
    (void)tv_sec_lo;
    (void)tv_nsec;
    struct capture_ctx *ctx = data;

    if (ctx->needs_conversion) {
        void *conv_pixels = NULL;

        size_t conv_size = 0;

        struct wl_buffer *conv_buffer = shm_buffer_create(
            ctx->state->shm, (int)ctx->raw_width, (int)ctx->raw_height, WL_SHM_FORMAT_XRGB8888, &conv_pixels, &conv_size
        );

        if (!conv_buffer) {
            // fprintf(stderr, "capture: failed to allocate conversion buffer\n");
            MIRU_LOG("capture: failed to allocate conversion buffer");
            free_raw(ctx);
            ctx->done = true;
            ctx->ok = false;
            return;
        }

        uint8_t *dst = (uint8_t *)conv_pixels;
        const uint8_t *src = (const uint8_t *)ctx->raw_shm_data;
        int dst_stride = (int)ctx->raw_width * 4;

        for (uint32_t y = 0; y < ctx->raw_height; y++) {
            const uint8_t *src_row = src + (size_t)y * ctx->raw_stride;
            uint8_t *dst_row = dst + (size_t)y * dst_stride;
            for (uint32_t x = 0; x < ctx->raw_width; x++) {
                dst_row[x * 4 + 0] = src_row[x * 3 + 2];
                dst_row[x * 4 + 1] = src_row[x * 3 + 1];
                dst_row[x * 4 + 2] = src_row[x * 3 + 0];
                dst_row[x * 4 + 3] = 0xFF;
            }
        }

        free_raw(ctx);

        ctx->out->buffer = conv_buffer;
        ctx->out->shm_data = conv_pixels;
        ctx->out->shm_size = conv_size;
        ctx->out->format = WL_SHM_FORMAT_XRGB8888;
        ctx->out->stride = (uint32_t)dst_stride;
    }

    ctx->done = true;
    ctx->ok = true;
}

static void handle_failed(void *data, struct zwlr_screencopy_frame_v1 *frame)
{
    (void)frame;
    struct capture_ctx *ctx = data;
    // fprintf(stderr, "capture: compositor reported capture failed\n");
    MIRU_LOG("capture: compositor reported capture failed");
    free_raw(ctx);
    ctx->done = true;
    ctx->ok = false;
}

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = handle_buffer,
    .flags = handle_flags,
    .ready = handle_ready,
    .failed = handle_failed,
};

// maps a pixel of the on-screen image to its position in the screencopy buffer
static void screen_to_buffer_coord(
    int32_t transform,
    uint32_t width,
    uint32_t height,
    uint32_t x,
    uint32_t y,
    uint32_t *bx,
    uint32_t *by
)
{
    switch (transform) {
    case WL_OUTPUT_TRANSFORM_90:
        *bx = y;
        *by = width - 1 - x;
        break;
    case WL_OUTPUT_TRANSFORM_180:
        *bx = width - 1 - x;
        *by = height - 1 - y;
        break;
    case WL_OUTPUT_TRANSFORM_270:
        *bx = height - 1 - y;
        *by = x;
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
        *bx = width - 1 - x;
        *by = y;
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        *bx = y;
        *by = x;
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        *bx = x;
        *by = height - 1 - y;
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        *bx = height - 1 - y;
        *by = width - 1 - x;
        break;
    default:
        *bx = x;
        *by = y;
        break;
    }
}

static int apply_output_transform(struct miru_state *state, struct miru_capture *capture, int32_t transform)
{
    if (transform == WL_OUTPUT_TRANSFORM_NORMAL)
        return 0;

    bool swap = (transform & 1) != 0; // 90, 270 and their flipped variants swap width/height
    uint32_t width = swap ? capture->height : capture->width;
    uint32_t height = swap ? capture->width : capture->height;

    void *pixels = NULL;
    size_t size = 0;
    struct wl_buffer *buffer = shm_buffer_create(state->shm, (int)width, (int)height, capture->format, &pixels, &size);
    if (!buffer) {
        MIRU_LOG("capture: failed to allocate buffer for output transform %d", transform);
        return -1;
    }

    uint8_t *dst = (uint8_t *)pixels;
    const uint8_t *src = (const uint8_t *)capture->shm_data;
    size_t dst_stride = (size_t)width * 4;

    for (uint32_t y = 0; y < height; y++) {
        uint8_t *dst_row = dst + ((size_t)y * dst_stride);
        for (uint32_t x = 0; x < width; x++) {
            uint32_t bx = 0;
            uint32_t by = 0;

            screen_to_buffer_coord(transform, width, height, x, y, &bx, &by);
            if (capture->y_invert)
                by = capture->height - 1 - by; // the result is stored top-down

            memcpy(dst_row + ((size_t)x * 4), src + ((size_t)by * capture->stride) + ((size_t)bx * 4), 4);
        }
    }

    capture_frame_destroy(capture);
    capture->buffer = buffer;
    capture->shm_data = pixels;
    capture->shm_size = size;
    capture->width = width;
    capture->height = height;
    capture->stride = (uint32_t)dst_stride;
    capture->y_invert = false;
    return 0;
}

int capture_output_frame(
    struct miru_state *state,
    const struct miru_output *output,
    volatile sig_atomic_t *cancel,
    struct miru_capture *out
)
{
    memset(out, 0, sizeof(*out)); // safe for callers to pass an uninitialized struct

    if (!output) {
        // fprintf(stderr, "capture: no output bound, compositor may be headless or output binding failed\n");
        MIRU_LOG("capture: no output bound, compositor may be headless or output binding failed");
        return -1;
    }

    // the dispatch loop below can run a global_remove that frees `output` so read it now
    int32_t transform = output->transform;

    struct capture_ctx ctx = { .state = state, .out = out, .done = false, .ok = false };

    struct zwlr_screencopy_frame_v1 *frame =
        zwlr_screencopy_manager_v1_capture_output(state->screencopy_manager, 0, output->wl_output);
    if (!frame) {
        // fprintf(stderr, "capture: failed to create screencopy frame\n");
        MIRU_LOG("capture: failed to create screencopy frame");
        return -1;
    }

    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, &ctx);

    while (!ctx.done) {
        if (cancel && *cancel) {
            // fprintf(stderr, "capture: cancelled before frame was ready\n");
            MIRU_LOG("capture: cancelled before frame was ready");
            free_raw(&ctx);
            zwlr_screencopy_frame_v1_destroy(frame);
            return -1;
        }
        if (wl_display_dispatch(state->display) == -1) {
            if (errno == EINTR) {
                continue; // loop back, cancel gets rechecked above next iteration
            }
            // fprintf(stderr, "capture: display dispatch failed while waiting for frame\n");
            MIRU_LOG("capture: display dispatch failed while waiting for frame");
            free_raw(&ctx);
            zwlr_screencopy_frame_v1_destroy(frame);
            return -1;
        }
    }

    zwlr_screencopy_frame_v1_destroy(frame);

    if (!ctx.ok) {
        capture_frame_destroy(out);
        return -1;
    }

    if (apply_output_transform(state, out, transform) != 0) {
        capture_frame_destroy(out);
        return -1;
    }
    return 0;
}

void capture_frame_destroy(struct miru_capture *capture)
{
    if (capture->shm_data) {
        shm_buffer_free(capture->shm_data, capture->shm_size);
        capture->shm_data = NULL;
        capture->shm_size = 0;
    }
    if (capture->buffer) {
        wl_buffer_destroy(capture->buffer);
        capture->buffer = NULL;
    }
}

int capture_refresh(
    struct miru_state *state,
    const struct miru_output *output,
    volatile sig_atomic_t *cancel,
    struct miru_capture *c
)
{
    if (!state || !c)
        return -1;

    capture_frame_destroy(c);
    return capture_output_frame(state, output, cancel, c);
}
