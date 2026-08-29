#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdbool.h>
#include "annotations.h"

struct miru_gl_renderer {
    GLuint program, texture, vbo;
    GLint a_position, a_texcoord;
    GLint u_texture, u_crop_origin, u_crop_scale, u_y_invert;
    GLint u_cursor_px, u_resolution;
    GLint u_spotlight_enabled, u_spotlight_radius, u_spotlight_softness, u_spotlight_dim;

    GLuint line_program, line_vbo;
    GLint line_a_pos, line_u_color;
};

int gl_renderer_init(struct miru_gl_renderer *r);
void gl_renderer_upload_texture(
    struct miru_gl_renderer *r,
    const uint8_t *pixels,
    int width,
    int height,
    int stride,
    uint32_t format
);
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

void gl_renderer_draw_annotations(
    struct miru_gl_renderer *r,
    const struct miru_annotation_state *ann,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    int viewport_w,
    int viewport_h
);

#endif
