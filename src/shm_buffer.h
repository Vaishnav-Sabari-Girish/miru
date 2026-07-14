#ifndef SHM_BUFFER_H
#define SHM_BUFFER_H

#include <wayland-client.h>
#include <stddef.h>

// allocates using an explicit stride (bytes per row), needed when a protocol
// (like screencopy) tells us exactly what layout to use, including any padding
struct wl_buffer *shm_buffer_create_stride(
    struct wl_shm *shm,
    int width,
    int height,
    int stride,
    uint32_t format,
    void **out_data,
    size_t *out_size
);

// convenience wrapper: assumes a tightly-packed 4-bytes-per-pixel stride (width * 4)
struct wl_buffer *
shm_buffer_create(struct wl_shm *shm, int width, int height, uint32_t format, void **out_data, size_t *out_size);

void shm_buffer_free(void *data, size_t size);

#endif
