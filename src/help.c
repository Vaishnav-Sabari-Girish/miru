#include "help.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#define HELP_MAX_LINES 48
#define HELP_LINE_LEN 128
#define HELP_LABEL_GAP 2

struct help_row {
    const char *raw;
    int indent;
    const char *label;
    enum miru_action act[4];
    int nact;
    bool all_binds; // list every binding of act[0] instead of only the first
    const char *desc;
};

// clang-format off
static const struct help_row rows[] = {
    {.raw = "Miru - controls"},
    {.raw = "LMB = Left Mouse Button"},
    {.raw = ""},
    {.label = "Mouse Move", .desc = "Pan"},
    {.act = {MIRU_ACT_PAN_LEFT, MIRU_ACT_PAN_RIGHT, MIRU_ACT_PAN_UP, MIRU_ACT_PAN_DOWN}, .nact = 4, .desc = "Pan"},
    {.label = "Scroll", .act = {MIRU_ACT_ZOOM_IN, MIRU_ACT_ZOOM_OUT}, .nact = 2, .desc = "Zoom In/Out"},
    {.act = {MIRU_ACT_TOGGLE_HIGHLIGHT}, .nact = 1, .desc = "Cursor Highlight"},
    {.act = {MIRU_ACT_RADIUS_UP, MIRU_ACT_RADIUS_DOWN}, .nact = 2, .desc = "Highlight Radius"},
    {.label = "Ctrl+Scroll", .desc = "Highlight Radius"},
    {.act = {MIRU_ACT_REFRESH}, .nact = 1, .desc = "Refresh freeze"},
    {.act = {MIRU_ACT_TOGGLE_CURSOR}, .nact = 1, .desc = "Toggle Cursor"},
    {.act = {MIRU_ACT_TOGGLE_ANNOTATE}, .nact = 1, .desc = "Annotation Mode"},
    {.indent = 1, .act = {MIRU_ACT_ANN_ARROW}, .nact = 1, .desc = "Arrow"},
    {.indent = 1, .act = {MIRU_ACT_ANN_RECT}, .nact = 1, .desc = "Rectangle"},
    {.indent = 1, .act = {MIRU_ACT_ANN_LINE}, .nact = 1, .desc = "Line"},
    {.indent = 1, .act = {MIRU_ACT_ANN_ELLIPSE}, .nact = 1, .desc = "Ellipse"},
    {.indent = 1, .act = {MIRU_ACT_ANN_TEXT}, .nact = 1, .desc = "Text"},
    {.indent = 1, .act = {MIRU_ACT_ANN_FREEHAND}, .nact = 1, .desc = "Freehand"},
    {.indent = 1, .label = "Drag LMB", .desc = "Place shape"},
    {.indent = 1, .label = "Click + type", .desc = "Place text"},
    {.indent = 3, .label = "Enter", .desc = "Commit Text"},
    {.indent = 3, .label = "ESC", .desc = "Cancel Text"},
    {.indent = 1, .act = {MIRU_ACT_UNDO}, .nact = 1, .desc = "Undo Annotation"},
    {.indent = 1, .act = {MIRU_ACT_REDO}, .nact = 1, .desc = "Redo Annotation"},
    {.indent = 1, .act = {MIRU_ACT_ANN_CLEAR}, .nact = 1, .desc = "Clear Annotations"},
    {.act = {MIRU_ACT_EXPORT}, .nact = 1, .desc = NULL},
    {.act = {MIRU_ACT_TOGGLE_BADGE}, .nact = 1, .desc = "Toggle Status Badge"},
    {.act = {MIRU_ACT_CYCLE_FILTER}, .nact = 1, .desc = "Cycle Color Filters"},
    {.act = {MIRU_ACT_HELP}, .nact = 1, .all_binds = true, .desc = "Toggle this help"},
    {.act = {MIRU_ACT_QUIT}, .nact = 1, .desc = "Close Help / Exit"},
};
// clang-format on

#define ROW_COUNT (sizeof(rows) / sizeof(rows[0]))
_Static_assert(ROW_COUNT <= HELP_MAX_LINES, "help panel has more rows than HELP_MAX_LINES");

static char line_buf[HELP_MAX_LINES][HELP_LINE_LEN];
static const char *line_ptrs[HELP_MAX_LINES];

static struct miru_keybinds keybinds;
static bool keybinds_ready;
static int export_format = 1;
static bool built;

static void append(char *dst, size_t size, const char *src)
{
    size_t len = strlen(dst);
    if (len < size)
        snprintf(dst + len, size - len, "%s", src);
}

static void build_label(const struct help_row *row, char *out, size_t size)
{
    out[0] = '\0';

    if (row->label)
        append(out, size, row->label);

    for (int i = 0; i < row->nact; i++) {
        enum miru_action act = row->act[i];
        int shown = row->all_binds ? keybinds.count[act] : (keybinds.count[act] > 0 ? 1 : 0);

        if (shown == 0) {
            if (out[0] == '\0')
                append(out, size, "/");

            append(out, size, "-");
            continue;
        }

        for (int b = 0; b < shown; b++) {
            char name[32];
            keybinds_format(&keybinds.binds[act][b], name, sizeof(name));
            if (out[0] != '\0')
                append(out, size, b > 0 ? " or " : "/");

            append(out, size, name);
        }
    }
}

static void rebuild(void)
{
    if (!keybinds_ready) {
        keybind_defaults(&keybinds);
        keybinds_ready = true;
    }

    static char labels[HELP_MAX_LINES][HELP_LINE_LEN];
    size_t column = 0;

    for (size_t i = 0; i < ROW_COUNT; i++) {
        const struct help_row *row = &rows[i];
        if (row->raw)
            continue;

        char label[64];

        build_label(row, label, sizeof(label));
        snprintf(labels[i], sizeof(labels[i]), "%*s%s", row->indent, "", label);

        if (strlen(labels[i]) > column)
            column = strlen(labels[i]);
    }

    column += HELP_LABEL_GAP;

    for (size_t i = 0; i < ROW_COUNT; i++) {
        const struct help_row *row = &rows[i];
        char *line = line_buf[i];

        if (row->raw) {
            snprintf(line, HELP_LINE_LEN, "%s", row->raw);
        } else {
            char desc[48];
            if (row->desc)
                snprintf(desc, sizeof(desc), "%s", row->desc);
            else
                snprintf(desc, sizeof(desc), "Export View (%s)", export_format == 1 ? "png" : "bmp");

            snprintf(line, HELP_LINE_LEN, "%s", labels[i]);

            for (size_t pad = strlen(labels[i]); pad < column; pad++)
                append(line, HELP_LINE_LEN, " ");

            append(line, HELP_LINE_LEN, desc);
        }

        line_ptrs[i] = line;
    }

    built = true;
}

const char *const *help_lines(void)
{
    if (!built)
        rebuild();

    return line_ptrs;
}

size_t help_line_count(void)
{
    return ROW_COUNT;
}

void help_set_export_format(int format)
{
    export_format = format;
    rebuild();
}

void help_set_keybinds(const struct miru_keybinds *kb)
{
    keybinds = *kb;
    keybinds_ready = true;
    rebuild();
}
