#include "annotations.h"
#include <stdio.h>
#include <string.h>

void annotation_state_init(struct miru_annotation_state *s)
{
    memset(s, 0, sizeof(*s));
    s->tool = MIRU_ANN_ARROW;
    s->redo_count = 0;

    s->shape_r = 1.0f;
    s->shape_g = 0.2f;
    s->shape_b = 0.2f;
    s->text_r = 1.0f;
    s->text_g = 0.85f;
    s->text_b = 0.2f;
    s->thickness = 4.0f;
    s->text_size = 1.0f;
}

void annotation_state_set_style(
    struct miru_annotation_state *s,
    int default_tool,
    float thickness,
    float shape_r,
    float shape_g,
    float shape_b,
    float text_r,
    float text_g,
    float text_b,
    float text_size
)
{
    if (!s)
        return;

    if (default_tool >= 0 && default_tool <= 5)
        s->tool = (enum miru_ann_type)default_tool;

    s->thickness = thickness > 0.f ? thickness : 4.0f;
    s->shape_r = shape_r;
    s->shape_g = shape_g;
    s->shape_b = shape_b;
    s->text_r = text_r;
    s->text_g = text_g;
    s->text_b = text_b;
    s->text_size = text_size >= 0.25f ? text_size : 1.0f;
}

void annotation_clear(struct miru_annotation_state *s)
{
    s->redo_count = 0;

    s->count = 0;
    s->dragging = false;
    s->typing = false;
    s->text_len = 0;
    s->text_buf[0] = '\0';
}

bool annotation_add(struct miru_annotation_state *s, enum miru_ann_type type, float x0, float y0, float x1, float y1)
{
    if (s->count >= MIRU_MAX_ANNOTATIONS)
        return false;

    struct miru_annotation *a = &s->items[s->count++];
    a->type = type;
    a->x0 = x0;
    a->x1 = x1;
    a->y0 = y0;
    a->y1 = y1;
    a->r = s->shape_r;
    a->g = s->shape_g;
    a->b = s->shape_b;
    a->a = 1.0f;

    a->npts = 0;

    a->thickness = s->thickness;
    s->redo_count = 0;
    return true;
}

void annotation_screen_to_buffer(
    float screen_x,
    float screen_y,
    float buf_w,
    float buf_h,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    float *out_bx,
    float *out_by
)
{
    float u = (buf_w > 1.f) ? (screen_x / buf_w) : 0.f;
    float v = (buf_h > 1.f) ? (screen_y / buf_h) : 0.f;
    *out_bx = src_left + u * src_w;
    *out_by = src_top + v * src_h;
}

void annotation_buffer_to_ndc(
    float bx,
    float by,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    float *out_nx,
    float *out_ny
)
{
    float u = (bx - src_left) / src_w;
    float v = (by - src_top) / src_h;

    *out_nx = u * 2.0f - 1.0f;
    *out_ny = 1.0f - v * 2.0f;
}

bool annotation_add_text(struct miru_annotation_state *s, float x, float y, const char *text)
{
    if (s->count >= MIRU_MAX_ANNOTATIONS || !text || !text[0])
        return false;

    struct miru_annotation *a = &s->items[s->count++];
    a->type = MIRU_ANN_TEXT;
    a->x0 = x;
    a->y0 = y;
    a->x1 = x;
    a->y1 = y;

    a->r = s->text_r;
    a->g = s->text_g;
    a->b = s->text_b;
    a->a = 1.0f;

    a->thickness = s->text_size;
    snprintf(a->text, MIRU_ANN_TEXT_MAX, "%s", text);
    s->redo_count = 0;
    return true;
}

char annotation_keycode_to_char(uint32_t key, bool shift)
{
    if (key >= 16 && key <= 25) {
        static const char row[] = "qwertyuiop";
        char c = row[key - 16];
        return shift ? (char)(c - 32) : c;
    }
    if (key >= 30 && key <= 38) {
        static const char row[] = "asdfghjkl";
        char c = row[key - 30];
        return shift ? (char)(c - 32) : c;
    }
    if (key >= 44 && key <= 50) {
        static const char row[] = "zxcvbnm";
        char c = row[key - 44];
        return shift ? (char)(c - 32) : c;
    }
    if (key >= 2 && key <= 11) {
        static const char n[] = "1234567890";
        static const char s[] = "!@#$%^&*()";
        int i = (int)(key - 2);
        return shift ? s[i] : n[i];
    }
    if (key == 57)
        return ' ';
    if (key == 12)
        return shift ? '_' : '-';
    if (key == 13)
        return shift ? '+' : '=';
    if (key == 52)
        return shift ? '>' : '.';
    if (key == 51)
        return shift ? '<' : ',';
    if (key == 53)
        return shift ? '?' : '/';

    if (key == 41)
        return shift ? '~' : '`';
    if (key == 39)
        return shift ? ':' : ';';
    if (key == 40)
        return shift ? '"' : '\'';
    if (key == 43)
        return shift ? '|' : '\\';
    if (key == 26)
        return shift ? '{' : '[';
    if (key == 27)
        return shift ? '}' : ']';

    if (key == 82)
        return '0';
    if (key == 79)
        return '1';
    if (key == 80)
        return '2';
    if (key == 81)
        return '3';
    if (key == 75)
        return '4';
    if (key == 76)
        return '5';
    if (key == 77)
        return '6';
    if (key == 71)
        return '7';
    if (key == 72)
        return '8';
    if (key == 73)
        return '9';
    if (key == 83)
        return '.';
    if (key == 74)
        return '-';
    if (key == 78)
        return '+';
    if (key == 55)
        return '*';
    if (key == 98)
        return '/';

    return 0;
}

bool annotation_undo(struct miru_annotation_state *s)
{
    if (!s || s->count <= 0)
        return false;

    if (s->redo_count >= MIRU_MAX_ANNOTATIONS)
        return false;

    s->redo_stack[s->redo_count++] = s->items[--s->count];
    return true;
}

bool annotation_redo(struct miru_annotation_state *s)
{
    if (!s || s->redo_count <= 0)
        return false;

    if (s->count >= MIRU_MAX_ANNOTATIONS)
        return false;

    s->items[s->count++] = s->redo_stack[--s->redo_count];
    return true;
}

bool annotation_add_freehand(struct miru_annotation_state *s, const float *xs, const float *ys, int n)
{
    if (!s || !xs || !ys || n < 2 || s->count >= MIRU_MAX_ANNOTATIONS)
        return false;

    if (n > MIRU_ANN_FREEHAND_MAX)
        n = MIRU_ANN_FREEHAND_MAX;

    struct miru_annotation *a = &s->items[s->count++];
    memset(a, 0, sizeof(*a));

    a->type = MIRU_ANN_FREEHAND;
    a->x0 = xs[0];
    a->x1 = xs[n - 1];
    a->y0 = ys[0];
    a->y1 = ys[n - 1];
    a->r = s->shape_r;
    a->g = s->shape_g;
    a->b = s->shape_b;
    a->a = 1.0f;

    a->thickness = s->thickness > 0.f ? s->thickness : 4.0f;
    a->npts = n;
    for (int i = 0; i < n; i++) {
        a->pts_x[i] = xs[i];
        a->pts_y[i] = ys[i];
    }
    s->redo_count = 0;
    return true;
}
