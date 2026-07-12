#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include "shm_buffer.h"

struct wl_buffer *shm_buffer_create(struct wl_shm *shm, int width, int height,
                                    uint32_t format, void **out_data, size_t *out_size) {
  if (width <= 0 || height <= 0) {
    fprintf(stderr, "shm_buffer_create: invalid dimensions %dx%d\n", width, height);
    return NULL;
  }

  // stride and total size both feed into int32_t Wayland protocol fields,
  // so an oversized configure could otherwise overflow silently
  if (width > (INT32_MAX / 4)) {
    fprintf(stderr, "shm_buffer_create: width %d too large\n", width);
    return NULL;
  }
  int stride = width * 4;

  if ((int64_t)stride * (int64_t)height > INT32_MAX) {
    fprintf(stderr, "shm_buffer_create: %dx%d buffer exceeds INT32_MAX bytes\n", width, height);
    return NULL;
  }
  size_t size = (size_t)stride * (size_t)height;

  int fd = memfd_create("miru-shm-buffer", 0);
  if (fd < 0) {
    perror("memfd_create");
    return NULL;
  }

  if (ftruncate(fd, (off_t)size) < 0) {
    perror("ftruncate");
    close(fd);
    return NULL;
  }

  void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return NULL;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
  close(fd);   // pool holds its own reference now regardless of success

  if (!pool) {
    fprintf(stderr, "wl_shm_create_pool failed\n");
    munmap(data, size);
    return NULL;
  }

  struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
  wl_shm_pool_destroy(pool);   // buffer keeps working after this either way

  if (!buffer) {
    fprintf(stderr, "wl_shm_pool_create_buffer failed\n");
    munmap(data, size);   // nothing else owns this mapping if the buffer never came into being
    return NULL;
  }

  *out_data = data;
  if (out_size) {
    *out_size = size;
  }
  return buffer;
}

void shm_buffer_free(void *data, size_t size) {
  if (data && size) {
    munmap(data, size);
  }
}
