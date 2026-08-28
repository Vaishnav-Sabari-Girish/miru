#ifndef ANNOTATIONS_H
#define ANNOTATIONS_H

#include <stdbool.h>

#define MIRU_MAX_ANNOTATIONS 64

enum miru_ann_type {
    MIRU_ANN_ARROW = 0,
    MIRU_ANN_RECT = 1,
};

struct miru_annotation {
    enum miru_ann_type type;
    float x0, y0; // buffer-space start (drag origin)
    float x1, y1; // buffer-space end   (drag release)
    float r, g, b, a;
    float thickness;
};

struct miru_annotation_state {
    struct miru_annotation items[MIRU_MAX_ANNOTATIONS];
    int count;

    bool mode; // annotate mode active
    enum miru_ann_type tool;

    bool dragging;
    float drag_x0, drag_y0;
    float drag_x1, drag_y1;
};

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

#endif // !ANNOTATIONS_H
