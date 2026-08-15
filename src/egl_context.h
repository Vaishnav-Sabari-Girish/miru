#ifndef EGL_CONTEXT_H
#define EGL_CONTEXT_H

#include <EGL/egl.h>
#include <wayland-client-core.h>
#include <wayland-egl-core.h>
#include <wayland-egl.h>
#include <wayland-client.h>

struct miru_egl {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    struct wl_egl_window *egl_window;
};

int egl_init(struct miru_egl *egl, struct wl_display *wl_display);
int egl_create_surface(struct miru_egl *egl, struct wl_surface *wl_surface, int width, int height);
void egl_resize_surface(struct miru_egl *egl, int width, int height);
void egl_destroy_surface(struct miru_egl *egl);
void egl_swap_buffers(struct miru_egl *egl);
void egl_cleanup(struct miru_egl *egl);

#endif
