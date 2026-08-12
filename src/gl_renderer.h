#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdbool.h>

struct miru_gl_renderer {
    GLuint program, texture, vbo;
    GLint a_position, a_texcoord;
    GLint u_texture, u_crop_origin, u_crop_scale, u_y_invert;
    GLint u_cursor_px, u_resolution;
    GLint u_spotlight_enabled, u_spotlight_radius, u_spotlight_softness, u_spotlight_dim;
};

int gl_renderer_init(struct miru_gl_renderer *r);
void gl_renderer_upload_texture(struct miru_gl_renderer *r, const uint8_t *pixels, int width, int height, int stride);
void gl_renderer_draw(
    struct miru_gl_renderer *r,
    float crop_x,
    float crop_y,
    float crop_w,
    float crop_h,
    int y_invert,
    float cursor_px_x,
    float cursor_px_y,
    int viewport_w,
    int viewport_h,
    bool spotlight_enabled,
    float spotlight_radius,
    float spotlight_softness,
    float spotlight_dim
);

void gl_renderer_cleanup(struct miru_gl_renderer *r);

#endif
