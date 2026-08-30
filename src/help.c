#include "help.h"
#include <stddef.h>

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
    "SHIFT + A         Annotation Mode",
    " W                Arrow",
    " R                Rectangle",
    " Drag LMB         Place Shape",
    " C                Clear Annotations",
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
