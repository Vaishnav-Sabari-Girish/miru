#ifndef KEYBINDS_H
#define KEYBINDS_H

#include "toml.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

struct toml_table;

#define MIRU_MOD_SHIFT 0x1
#define MIRU_MOD_CTRL 0x2

#define MIRU_KB_MAX_ALT 4

enum miru_action {
    MIRU_ACT_QUIT,
    MIRU_ACT_HELP,
    MIRU_ACT_EXPORT,
    MIRU_ACT_REFRESH,
    MIRU_ACT_TOGGLE_CURSOR,
    MIRU_ACT_TOGGLE_HIGHLIGHT,
    MIRU_ACT_TOGGLE_ANNOTATE,
    MIRU_ACT_TOGGLE_BADGE,
    MIRU_ACT_CYCLE_FILTER,
    MIRU_ACT_ZOOM_IN,
    MIRU_ACT_ZOOM_OUT,
    MIRU_ACT_RADIUS_UP,
    MIRU_ACT_RADIUS_DOWN,
    MIRU_ACT_PAN_LEFT,
    MIRU_ACT_PAN_RIGHT,
    MIRU_ACT_PAN_UP,
    MIRU_ACT_PAN_DOWN,
    MIRU_ACT_ANN_ARROW,
    MIRU_ACT_ANN_ELLIPSE,
    MIRU_ACT_ANN_LINE,
    MIRU_ACT_ANN_RECT,
    MIRU_ACT_ANN_TEXT,
    MIRU_ACT_ANN_FREEHAND,
    MIRU_ACT_ANN_CLEAR,
    MIRU_ACT_UNDO,
    MIRU_ACT_REDO,
    MIRU_ACT_COUNT,
};

struct miru_bind {
    uint32_t key; //evdev keycode (KEY_*)
    uint8_t mods; // MIRU_MOD_* bitmask
};

struct miru_keybinds {
    struct miru_bind binds[MIRU_ACT_COUNT][MIRU_KB_MAX_ALT];
    uint8_t count[MIRU_ACT_COUNT];
};

void keybind_defaults(struct miru_keybinds *kb);

void keybinds_load(struct miru_keybinds *kb, const struct toml_table *t);

bool keybinds_match(const struct miru_keybinds *kb, enum miru_action action, uint32_t key, uint8_t mods);

bool keybinds_equal(const struct miru_keybinds *a, const struct miru_keybinds *b);

// Writes a complete `[keybinds]` section in the config file with the default keybinds
void keybinds_write(FILE *f, const struct miru_keybinds *kb);

// human-readable binding (SHIFT+E etc)
void keybinds_format(const struct miru_bind *b, char *out, size_t out_size);

// Parses a list of comma separated keybinds like "SHIFT+E, SHIFT+F" and returns the
// number of bindings stored in `out` or -1 when the text is invalid
int keybinds_parse_list(const char *text, struct miru_bind *out, int max);

#endif // !KEYBINDS_H
