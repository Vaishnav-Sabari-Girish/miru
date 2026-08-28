#include "annotations.h"
#include <string.h>

void annotation_state_init(struct miru_annotation_state *s)
{
    memset(s, 0, sizeof(*s));
    s->tool = MIRU_ANN_ARROW;
}

void annotation_clear(struct miru_annotation_state *s)
{
    s->count = 0;
    s->dragging = false;
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
    a->r = 1.0f;
    a->g = 0.2f;
    a->b = 0.2f;
    a->a = 1.0f;

    a->thickness = 4.0f;
    return true;
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
