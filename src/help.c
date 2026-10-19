#include "help.h"
#include <stdio.h>
#include <stddef.h>

static char export_line[48] = "SHIFT+E           Export view (png)";

static const char *const lines[] = {
    "Miru - controls",
    "LMB = Left Mouse Button",
    "",
    "Mouse Move        Pan",
    "WASD/arrow keys   Pan",
    "Scroll/+/-        Zoom in/out",
    "Tab               Cursor Highlight",
    "SHIFT+/-          Highlight radius",
    "CTRL+Scroll       Highlight radius",
    "SHIFT + R         refresh freeze",
    "SHIFT + C         Toggle cursor",
    "SHIFT + A         Annotation Mode",
    " W                Arrow",
    " R                Rectangle",
    " L                Line",
    " O                Ellipse",
    " T                Text",
    " Drag LMB         Place Shape",
    " Click + type     Place Text",
    "   Enter          Commit Text",
    "   ESC            Cancel Text",
    " CTRL+Z           Undo Annotation",
    " CTRL+Y           Redo Annotation",
    " C                Clear Annotations",
    export_line,
    "F                 Cycle Color filters",
    "SHIFT+H or ?      Toggle this help",
    "ESC               Close help / exit",
};

const char *const *help_lines(void)
{
    return lines;
}

size_t help_line_count()
{
    return sizeof(lines) / sizeof(lines[0]);
}

void help_set_export_format(int format)
{
    snprintf(export_line, sizeof(export_line), "SHIFT+E           Export view (%s)", format == 1 ? "png" : "bmp");
}
