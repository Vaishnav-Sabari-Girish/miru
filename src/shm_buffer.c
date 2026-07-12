#define _GNU_SOURCE                     // must come before any system headers, needed for memfd_create
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "shm_buffer.h"

struct wl_buffer *shm_buffer_create(struct wl_shm *shm, int width, int height,
                                    uint32_t format, void **out_data, size_t *out_size) {
  int stride = width * 4;                                       // 4 bytes/pixel for ARGB8888 & friends
  size_t size = (size_t)stride * (size_t)height;

  int fd = memfd_create("miru-shm-buffer", 0);                  // anonymous RAM-backed file, no disk entry
  if (fd < 0) {
    perror("memfd_create");
    return NULL;
  }

  if (ftruncate(fd, (off_t)size) < 0) {                         // resize the anonymous file to fit the buffer
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
  close(fd);                                                     // pool holds its own reference now

  struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
  wl_shm_pool_destroy(pool);                                     // buffer keeps working after this

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
