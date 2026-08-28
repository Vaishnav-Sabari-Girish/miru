#include "annotations.h"
#include <GLES2/gl2.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gl_renderer.h"
#include "debug.h"
#include <wayland-client-protocol.h>

/* bool miru_debug_enabled(void) */
/* { */
/*     static int cached = -1; */
/*     if (cached == -1) { */
/*         const char *v = getenv("MIRU_DEBUG"); */
/*         cached = (v && *v && strcmp(v, "0") != 0) ? 1 : 0; */
/*     } */

/*     return cached != 0; */
/* } */

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif /* ifndef MACRO */

#define DRM_FORMAT_ARGB8888 0x34325241
#define DRM_FORMAT_XRGB8888 0x34325258

static const char *vertex_shader_src = "attribute vec2 a_position;\n"
                                       "attribute vec2 a_texcoord;\n"
                                       "varying vec2 v_texcoord;\n"
                                       "void main() {\n"
                                       "    v_texcoord = a_texcoord;\n"
                                       "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                       "}\n";

static const char *fragment_shader_src = "precision mediump float;\n"
                                         "varying vec2 v_texcoord;\n"
                                         "uniform sampler2D u_texture;\n"
                                         "uniform vec2 u_crop_origin;\n"
                                         "uniform vec2 u_crop_scale;\n"
                                         "uniform float u_y_invert;\n"
                                         "uniform vec2 u_cursor_px;\n"
                                         "uniform vec2 u_resolution;\n"
                                         "uniform float u_spotlight_enabled;\n"
                                         "uniform float u_spotlight_radius;\n"
                                         "uniform float u_spotlight_softness;\n"
                                         "uniform float u_spotlight_dim;\n"
                                         "void main() {\n"
                                         "    vec2 uv = v_texcoord;\n"
                                         "    if (u_y_invert > 0.5) { uv.y = 1.0 - uv.y; }\n"
                                         "    vec2 sample_uv = u_crop_origin + uv * u_crop_scale;\n"
                                         "    vec3 color = texture2D(u_texture, sample_uv).rgb;\n"
                                         "    if (u_spotlight_enabled > 0.5) {\n"
                                         "        vec2 frag_px = gl_FragCoord.xy;\n"
                                         "        float dist = distance(frag_px, u_cursor_px);\n"
                                         "        float inner = max(u_spotlight_radius - u_spotlight_softness , 0.0);\n"
                                         "        float outer = u_spotlight_radius + u_spotlight_softness;\n"
                                         "        float t = smoothstep(inner, outer, dist);\n"
                                         "        color *= (1.0 - u_spotlight_dim * t);\n"
                                         "    }\n"
                                         "    gl_FragColor = vec4(color, 1.0);\n"
                                         "}\n";

static const char *line_vs = "attribute vec2 a_position;\n"
                             "void main() {\n"
                             "   gl_Position = vec4(a_position, 0.0, 1.0);\n"
                             "}\n";

static const char *line_fs = "precision mediump float;\n"
                             "uniform vec4 u_color;\n"
                             "void main() {\n"
                             "   gl_FragColor = u_color;\n"
                             "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        if (miru_debug_enabled()) {
            fprintf(stderr, "gl_renderer: shader compile failed: %s\n", log);
        }

        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

int gl_renderer_init(struct miru_gl_renderer *r)
{
    memset(r, 0, sizeof(*r));

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);

    if (!vs || !fs)
        return -1;

    r->program = glCreateProgram();
    glAttachShader(r->program, vs);
    glAttachShader(r->program, fs);

    glLinkProgram(r->program);

    GLint linked = 0;
    glGetProgramiv(r->program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        char log[512];
        glGetProgramInfoLog(r->program, sizeof(log), NULL, log);
        fprintf(stderr, "gl_renderer: program link failed: %s\n", log);
        return -1;
    }
    r->a_position = glGetAttribLocation(r->program, "a_position");
    r->a_texcoord = glGetAttribLocation(r->program, "a_texcoord");
    r->u_texture = glGetUniformLocation(r->program, "u_texture");
    r->u_crop_origin = glGetUniformLocation(r->program, "u_crop_origin");
    r->u_crop_scale = glGetUniformLocation(r->program, "u_crop_scale");
    r->u_y_invert = glGetUniformLocation(r->program, "u_y_invert");
    r->u_cursor_px = glGetUniformLocation(r->program, "u_cursor_px");
    r->u_resolution = glGetUniformLocation(r->program, "u_resolution");
    r->u_spotlight_enabled = glGetUniformLocation(r->program, "u_spotlight_enabled");
    r->u_spotlight_dim = glGetUniformLocation(r->program, "u_spotlight_dim");
    r->u_spotlight_radius = glGetUniformLocation(r->program, "u_spotlight_radius");
    r->u_spotlight_softness = glGetUniformLocation(r->program, "u_spotlight_softness");

    float verts[] = {
        // x, y (clip space), u, v
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
    };

    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glGenTextures(1, &r->texture);
    glBindTexture(GL_TEXTURE_2D, r->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint lvs = compile_shader(GL_VERTEX_SHADER, line_vs);
    GLuint lfs = compile_shader(GL_FRAGMENT_SHADER, line_fs);

    if (!lvs || !lfs) {
        return -1;
    }

    r->line_program = glCreateProgram();
    glAttachShader(r->line_program, lvs);
    glAttachShader(r->line_program, lfs);
    glLinkProgram(r->line_program);
    glDeleteShader(lvs);
    glDeleteShader(lfs);

    GLint ok = 0;
    glGetProgramiv(r->line_program, GL_LINK_STATUS, &ok);

    if (!ok)
        return -1;

    r->line_a_pos = glGetAttribLocation(r->line_program, "a_position");
    r->line_u_color = glGetUniformLocation(r->line_program, "u_color");
    glGenBuffers(1, &r->line_vbo);

    return 0;
}

void gl_renderer_upload_texture(
    struct miru_gl_renderer *r,
    const uint8_t *pixels,
    int width,
    int height,
    int stride,
    uint32_t format
)
{
    fprintf(
        stderr,
        "gl_renderer: upload width=%d height=%d stride=%d (width*4=%d) %s path\n",
        width,
        height,
        stride,
        width * 4,
        (stride == width * 4) ? "FAST" : "ROW BY ROW"
    );

    GLenum gl_fmt = GL_RGBA;
    bool need_swizzle = false;

    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        // little-endian BGRA in memory
        gl_fmt = GL_BGRA_EXT;
        break;
    case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XBGR8888:
        gl_fmt = GL_RGBA;
        break;
    default:
        gl_fmt = GL_RGBA;
        break;
    }

    glBindTexture(GL_TEXTURE_2D, r->texture);
    if (stride == width * 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, gl_fmt, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, gl_fmt, GL_UNSIGNED_BYTE, NULL);
        for (int y = 0; y < height; y++) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, gl_fmt, GL_UNSIGNED_BYTE, pixels + (size_t)y * stride);
        }
    }
}

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
)
{
    glViewport(0, 0, viewport_w, viewport_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(r->program);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glEnableVertexAttribArray(r->a_position);
    glVertexAttribPointer(r->a_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(r->a_texcoord);
    glVertexAttribPointer(r->a_texcoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->texture);
    glUniform1i(r->u_texture, 0);
    glUniform2f(r->u_crop_origin, crop_x, crop_y);
    glUniform2f(r->u_crop_scale, crop_w, crop_h);
    glUniform1f(r->u_y_invert, y_invert ? 1.0f : 0.0f);
    glUniform2f(r->u_cursor_px, cursor_px_x, cursor_px_y);
    glUniform2f(r->u_resolution, (float)viewport_w, (float)viewport_h);
    glUniform1f(r->u_spotlight_enabled, spotlight_enabled ? 1.0f : 0.0f);
    glUniform1f(r->u_spotlight_radius, spotlight_radius);
    glUniform1f(r->u_spotlight_softness, spotlight_softness);
    glUniform1f(r->u_spotlight_dim, spotlight_dim);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void gl_renderer_cleanup(struct miru_gl_renderer *r)
{
    if (r->texture)
        glDeleteTextures(1, &r->texture);
    if (r->vbo)
        glDeleteBuffers(1, &r->vbo);
    if (r->program)
        glDeleteProgram(r->program);
    if (r->line_vbo) {
        glDeleteBuffers(1, &r->line_vbo);
    }
    if (r->line_program) {
        glDeleteProgram(r->line_program);
    }
    memset(r, 0, sizeof(*r));
}

static void emit_line(
    struct miru_gl_renderer *r,
    float x0,
    float y0,
    float x1,
    float y1,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    float cr,
    float cg,
    float cb,
    float ca,
    float thickness_px,
    int viewport_w
)
{
    float nx0, ny0, nx1, ny1;

    annotation_buffer_to_ndc(x0, y0, src_left, src_top, src_w, src_h, &nx0, &ny0);
    annotation_buffer_to_ndc(x1, y1, src_left, src_top, src_w, src_h, &nx1, &ny1);

    float lw = thickness_px * ((float)viewport_w / (src_w > 1.f ? src_w : 1.f));
    if (lw < 1.f)
        lw = 1.f;
    if (lw > 8.f)
        lw = 8.f;

    glLineWidth(lw);

    float verts[] = { nx0, ny0, nx1, ny1 };
    glUniform4f(r->line_u_color, cr, cg, cb, ca);
    glBindBuffer(GL_ARRAY_BUFFER, r->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(r->line_a_pos);
    glVertexAttribPointer(r->line_a_pos, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glDrawArrays(GL_LINES, 0, 2);
}

static void emit_arrow(
    struct miru_gl_renderer *r,
    float x0,
    float y0,
    float x1,
    float y1,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    float cr,
    float cg,
    float cb,
    float ca,
    float thickness,
    int viewport_w
)
{
    emit_line(r, x0, y0, x1, y1, src_left, src_top, src_w, src_h, cr, cg, cb, ca, thickness, viewport_w);

    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.f)
        return;

    dx /= len;
    dy /= len;

    float head = thickness * 4.0f;
    if (head < 12.f)
        head = 12.f;

    float px = -dy, py = dx; // Perpendicular
    float ax = x1 - dx * head + px * head * 0.5f;
    float ay = y1 - dy * head + py * head * 0.5f;
    float bx = x1 - dx * head - px * head * 0.5f;
    float by = y1 - dy * head - py * head * 0.5f;

    emit_line(r, x1, y1, ax, ay, src_left, src_top, src_w, src_h, cr, cg, cb, ca, thickness, viewport_w);
    emit_line(r, x1, y1, bx, by, src_left, src_top, src_w, src_h, cr, cg, cb, ca, thickness, viewport_w);
}

void gl_renderer_draw_annotations(
    struct miru_gl_renderer *r,
    const struct miru_annotation_state *ann,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    int viewport_w,
    int viewport_h
)
{
    (void)viewport_h;
    if (!ann)
        return;

    if (ann->count == 0 && !ann->dragging && !ann->mode)
        return;

    glUseProgram(r->line_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (ann->mode) {
        float t = 6.f;
        float L = 0.f;
        float T = 0.f;
        float R = src_left + src_w;
        float B = src_top + src_h;

        emit_line(r, src_left, src_top, R, src_top, src_left, src_top, src_w, src_h, 1.f, 0.5f, 0.f, 1.f, t, viewport_w);
        emit_line(r, R, src_top, R, B, src_left, src_top, src_w, src_h, 1.f, 0.5f, 0.f, 1.f, t, viewport_w);
        emit_line(r, R, B, src_left, B, src_left, src_top, src_w, src_h, 1.f, 0.5f, 0.f, 1.f, t, viewport_w);
        emit_line(
            r, src_left, B, src_left, src_top, src_left, src_top, src_w, src_h, 1.f, 0.5f, 0.f, 1.f, t, viewport_w
        );
    }

    for (int i = 0; i < ann->count; i++) {
        const struct miru_annotation *a = &ann->items[i];

        if (a->type == MIRU_ANN_ARROW) {
            emit_arrow(
                r,
                a->x0,
                a->y0,
                a->x1,
                a->y1,
                src_left,
                src_top,
                src_w,
                src_h,
                a->r,
                a->g,
                a->b,
                a->a,
                a->thickness,
                viewport_w
            );
        } else if (a->type == MIRU_ANN_RECT) {
            emit_line(
                r,
                a->x0,
                a->y0,
                a->x1,
                a->y0,
                src_left,
                src_top,
                src_w,
                src_h,
                a->r,
                a->g,
                a->b,
                a->a,
                a->thickness,
                viewport_w
            );
            emit_line(
                r,
                a->x1,
                a->y0,
                a->x1,
                a->y1,
                src_left,
                src_top,
                src_w,
                src_h,
                a->r,
                a->g,
                a->b,
                a->a,
                a->thickness,
                viewport_w
            );
            emit_line(
                r,
                a->x1,
                a->y1,
                a->x0,
                a->y1,
                src_left,
                src_top,
                src_w,
                src_h,
                a->r,
                a->g,
                a->b,
                a->a,
                a->thickness,
                viewport_w
            );
            emit_line(
                r,
                a->x0,
                a->y1,
                a->x0,
                a->y0,
                src_left,
                src_top,
                src_w,
                src_h,
                a->r,
                a->g,
                a->b,
                a->a,
                a->thickness,
                viewport_w
            );
        }
    }

    if (ann->dragging) {
        float x0 = ann->drag_x0;
        float y0 = ann->drag_y0;
        float x1 = ann->drag_x1;
        float y1 = ann->drag_y1;

        if (ann->tool == MIRU_ANN_ARROW) {
            emit_arrow(r, x0, y0, x1, y1, src_left, src_top, src_w, src_h, 1.0f, 0.85f, 0.2f, 1.f, 4.f, viewport_w);
        } else {
            emit_line(r, x0, y0, x1, y0, src_left, src_top, src_w, src_h, 1.0f, 0.85f, 0.2f, 1.f, 4.f, viewport_w);
            emit_line(r, x1, y0, x1, y1, src_left, src_top, src_w, src_h, 1.0f, 0.85f, 0.2f, 1.f, 4.f, viewport_w);
            emit_line(r, x1, y1, x0, y1, src_left, src_top, src_w, src_h, 1.0f, 0.85f, 0.2f, 1.f, 4.f, viewport_w);
            emit_line(r, x0, y1, x0, y0, src_left, src_top, src_w, src_h, 1.0f, 0.85f, 0.2f, 1.f, 4.f, viewport_w);
        }
    }
}
