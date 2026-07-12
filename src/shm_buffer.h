#ifndef SHM_BUFFER_H
#define SHM_BUFFER_H

#include <wayland-client.h>
#include <stddef.h>

// allocates an anonymous shared memory region and wraps it as a wl_buffer the compositor can read
// out_data receives the mmap'd pointer, so the caller can write pixels straight into it
// out_size receives the byte size, needed later to munmap it (pass NULL if you don't care)
struct wl_buffer *shm_buffer_create(
    struct wl_shm *shm,
    int width,
    int height,
    uint32_t format,
    void **out_data,
    size_t *out_size
);

// unmaps memory returned above, does not touch the wl_buffer object itself
void shm_buffer_free(void *data, size_t size);

#endif // !SHM_BUFFER_H
