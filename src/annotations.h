#ifndef ANNOTATIONS_H
#define ANNOTATIONS_H

#define MIRU_ANN_TEXT_MAX 128

#include <stdbool.h>
#include <stdint.h>

#define MIRU_MAX_ANNOTATIONS 64

enum miru_ann_type {
    MIRU_ANN_ARROW = 0,
    MIRU_ANN_RECT = 1,
    MIRU_ANN_TEXT = 2,
};

struct miru_annotation {
    enum miru_ann_type type;
    float x0, y0; // buffer-space start (drag origin)
    float x1, y1; // buffer-space end   (drag release)
    float r, g, b, a;
    float thickness;
    char text[MIRU_ANN_TEXT_MAX];
};

struct miru_annotation_state {
    struct miru_annotation items[MIRU_MAX_ANNOTATIONS];
    int count;

    bool mode; // annotate mode active
    enum miru_ann_type tool;

    bool dragging;
    float drag_x0, drag_y0;
    float drag_x1, drag_y1;
    float hover_x, hover_y;

    bool typing;
    float text_x, text_y;
    char text_buf[MIRU_ANN_TEXT_MAX];
    int text_len;
};

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
);

void annotation_state_init(struct miru_annotation_state *s);
void annotation_clear(struct miru_annotation_state *s);
bool annotation_add(struct miru_annotation_state *s, enum miru_ann_type type, float x0, float y0, float x1, float y1);

void annotation_buffer_to_ndc(
    float bx,
    float by,
    float src_left,
    float src_top,
    float src_w,
    float src_h,
    float *out_nx,
    float *out_ny
);

bool annotation_add_text(struct miru_annotation_state *s, float x, float y, const char *text);
char annotation_keycode_to_char(uint32_t key, bool shift);

#endif // !ANNOTATIONS_H
