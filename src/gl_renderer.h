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
    GLint u_texel_size;
    GLint u_upscale;
    int tex_w, tex_h;
    int upscale_mode;

    GLuint line_program, line_vbo;
    GLint line_a_pos, line_u_color;

    GLuint spotlight_program;
    GLint spotlight_a_pos;
    GLint spotlight_u_cursor;
    GLint spotlight_u_resolution;
    GLint spotlight_u_radius;
    GLint spotlight_u_softness;
    GLint spotlight_u_dim;
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

void gl_renderer_set_upscale(struct miru_gl_renderer *r, int mode);

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

void gl_renderer_draw_help(struct miru_gl_renderer *r, int viewport_w, int viewport_h);

void gl_renderer_draw_loupe(
    struct miru_gl_renderer *r,
    float src_x0,
    float src_y0,
    float src_x1,
    float src_y1,
    float dst_x0,
    float dst_y0,
    float dst_x1,
    float dst_y1,
    int buf_w,
    int buf_h,
    int y_invert
);

void gl_renderer_draw_loupe_outline(
    struct miru_gl_renderer *r,
    float x0,
    float y0,
    float x1,
    float y1,
    int buf_w,
    int buf_h
);

void gl_renderer_draw_standalone_spotlight(
    struct miru_gl_renderer *r,
    float cursor_px_x,
    float cursor_px_y,
    float viewport_w,
    float viewport_h,
    float radius,
    float softness,
    float dim
);

#endif
