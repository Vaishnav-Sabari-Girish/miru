#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <stdio.h>
#include <string.h>
#include <wayland-egl-core.h>
#include <wayland-util.h>
#include "egl_context.h"

int egl_init(struct miru_egl *egl, struct wl_display *wl_display)
{
    memset(egl, 0, sizeof(*egl));

    egl->display = eglGetDisplay((EGLNativeDisplayType)wl_display);
    if (egl->display == EGL_NO_DISPLAY) {
        fprintf(stderr, "egl: eglGetDisplay failed\n");
        return -1;
    }

    EGLint major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
        fprintf(stderr, "egl: eglInitialize failed\n");
        return -1;
    }

    fprintf(stderr, "egl: Initialized , version %d.%d\n", major, minor);

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "egl: eglBindAPI failed\n");
        return -1;
    }

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };

    EGLint num_configs = 0;
    if (!eglChooseConfig(egl->display, config_attribs, &egl->config, 1, &num_configs) || num_configs < 1) {
        fprintf(stderr, "egl: eglChooseConfig failed, no matching config\n");
        return -1;
    }

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, context_attribs);
    if (egl->context == EGL_NO_CONTEXT) {
        fprintf(stderr, "egl: eglCreateContext failed\n");
        return -1;
    }

    return 0;
}

int egl_create_surface(struct miru_egl *egl, struct wl_surface *wl_surface, int width, int height)
{
    egl->egl_window = wl_egl_window_create(wl_surface, width, height);
    if (!egl->egl_window) {
        fprintf(stderr, "egl: wl_egl_window_create failed\n");
        return -1;
    }

    egl->surface = eglCreateWindowSurface(egl->display, egl->config, (EGLNativeWindowType)egl->egl_window, NULL);
    if (egl->surface = EGL_NO_SURFACE) {
        fprintf(stderr, "egl: eglCreateWindowSurface failed\n");
        wl_egl_window_destroy(egl->egl_window);
        egl->egl_window = NULL;
        return -1;
    }

    if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
        fprintf(stderr, "egl: eglMakeCurrent failed\n");
        return -1;
    }

    return 0;
}

void egl_resize_surface(struct miru_egl *egl, int width, int height)
{
    if (egl->egl_window) {
        wl_egl_window_resize(egl->egl_window, width, height, 0, 0);
    }
}

void egl_destroy_surface(struct miru_egl *egl)
{
    if (egl->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (egl->surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl->display, egl->surface);
        egl->surface = EGL_NO_SURFACE;
    }

    if (egl->egl_window) {
        wl_egl_window_destroy(egl->egl_window);
        egl->egl_window = NULL;
    }
}

void egl_swap_buffers(struct miru_egl *egl)
{
    eglSwapBuffers(egl->display, egl->surface);
}

void egl_cleanup(struct miru_egl *egl)
{
    egl_destroy_surface(egl);

    if (egl->context != EGL_NO_CONTEXT) {
        eglDestroyContext(egl->display, egl->context);
        egl->context = EGL_NO_CONTEXT;
    }
    if (egl->display != EGL_NO_DISPLAY) {
        eglTerminate(egl->display);
        egl->display = EGL_NO_DISPLAY;
    }
}
