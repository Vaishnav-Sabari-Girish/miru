#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "gl_renderer.h"

static const char *vertex_shader_src = "attribute vec2 a_position;\n"
                                       "attribute vec2 a_texcoord;\n"
                                       "varying vec2 v_texcoord;\n"
                                       "void main() {\n"
                                       "    v_texcoord = a_texcoord;\n"
                                       "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                       "}\n";

static const char *fragment_shader_src =
    "precision mediump float;\n"
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
    "    vec3 color = texture2D(u_texture, sample_uv).bgr;\n"
    "    if (u_spotlight_enabled > 0.5) {\n"
    "        vec2 frag_px = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);\n"
    "        float dist = distance(frag_px, u_cursor_px);\n"
    "        float inner = max(u_spotlight_radius - u_spotlight_softness , 0.0);\n"
    "        float outer = u_spotlight_radius + u_spotlight_softness;\n"
    "        float t = smoothstep(inner, outer, dist);\n"
    "        color *= (1.0 - u_spotlight_dim * t);\n"
    "    }\n"
    "    gl_FragColor = vec4(color, 1.0);\n"
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
        fprintf(stderr, "gl_renderer: shader compile failed: %s\n", log);
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
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
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

    return 0;
}

void gl_renderer_upload_texture(struct miru_gl_renderer *r, const uint8_t *pixels, int width, int height, int stride)
{
    glBindTexture(GL_TEXTURE_2D, r->texture);
    if (stride == width * 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        for (int y = 0; y < height; y++) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels + (size_t)y * stride);
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
    memset(r, 0, sizeof(*r));
}
