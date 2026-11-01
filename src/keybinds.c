#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <linux/input-event-codes.h>
#include "keybinds.h"
#include "debug.h"
#include "toml.h"

struct action_info {
    const char *name;
    const char *defaults;
};

static const struct action_info actions[MIRU_ACT_COUNT] = {
    [MIRU_ACT_QUIT] = { "quit", "ESC" },
    [MIRU_ACT_HELP] = { "help", "SHIFT+H, SHIFT+SLASH" },
    [MIRU_ACT_EXPORT] = { "export", "SHIFT+E" },
    [MIRU_ACT_REFRESH] = { "refresh", "SHIFT+R" },
    [MIRU_ACT_TOGGLE_CURSOR] = { "toggle_cursor", "SHIFT+C" },
    [MIRU_ACT_TOGGLE_HIGHLIGHT] = { "toggle_highlight", "TAB" },
    [MIRU_ACT_TOGGLE_ANNOTATE] = { "annotate", "SHIFT+A" },
    [MIRU_ACT_TOGGLE_PIP] = { "toggle_pip", "SHIFT+P" },
    [MIRU_ACT_TOGGLE_BADGE] = { "status_badge", "B" },
    [MIRU_ACT_CYCLE_FILTER] = { "cycle_filter", "F" },
    [MIRU_ACT_ZOOM_IN] = { "zoom_in", "EQUAL, SHIFT+EQUAL, KPPLUS" },
    [MIRU_ACT_ZOOM_OUT] = { "zoom_out", "MINUS, SHIFT+MINUS, KPMINUS" },
    [MIRU_ACT_RADIUS_UP] = { "radius_up", "CTRL+EQUAL, CTRL+SHIFT+EQUAL, CTRL+KPPLUS" },
    [MIRU_ACT_RADIUS_DOWN] = { "radius_down", "CTRL+MINUS, CTRL+SHIFT+MINUS, CTRL+KPMINUS" },
    [MIRU_ACT_PAN_LEFT] = { "pan_left", "LEFT, A" },
    [MIRU_ACT_PAN_RIGHT] = { "pan_right", "RIGHT, D" },
    [MIRU_ACT_PAN_UP] = { "pan_up", "UP, W" },
    [MIRU_ACT_PAN_DOWN] = { "pan_down", "DOWN, S" },
    [MIRU_ACT_ANN_ARROW] = { "tool_arrow", "W" },
    [MIRU_ACT_ANN_ELLIPSE] = { "tool_ellipse", "O" },
    [MIRU_ACT_ANN_LINE] = { "tool_line", "L" },
    [MIRU_ACT_ANN_RECT] = { "tool_rect", "R" },
    [MIRU_ACT_ANN_TEXT] = { "tool_text", "T" },
    [MIRU_ACT_ANN_FREEHAND] = { "tool_freehand", "P" },
    [MIRU_ACT_ANN_CLEAR] = { "clear", "C" },
    [MIRU_ACT_UNDO] = { "undo", "CTRL+Z" },
    [MIRU_ACT_REDO] = { "redo", "CTRL+Y, CTRL+SHIFT+Z" },
};

struct key_name {
    const char *name; // matched case-insensitively
    uint32_t code;
    const char *label; // Shown in the help panel, NULL means "Same as Name"
};
#define KB_LETTER(c) { #c, KEY_##c, NULL }

#define KB_DIGIT(d) { #d, KEY_##d, NULL }

#define KB_FKEY(f) { "F" #f, KEY_F##f, NULL }

static const struct key_name key_names[] = {
    KB_LETTER(A),
    KB_LETTER(B),
    KB_LETTER(C),
    KB_LETTER(D),
    KB_LETTER(E),
    KB_LETTER(F),
    KB_LETTER(G),
    KB_LETTER(H),
    KB_LETTER(I),
    KB_LETTER(J),
    KB_LETTER(K),
    KB_LETTER(L),
    KB_LETTER(M),
    KB_LETTER(N),
    KB_LETTER(O),
    KB_LETTER(P),
    KB_LETTER(Q),
    KB_LETTER(R),
    KB_LETTER(S),
    KB_LETTER(T),
    KB_LETTER(U),
    KB_LETTER(V),
    KB_LETTER(W),
    KB_LETTER(X),
    KB_LETTER(Y),
    KB_LETTER(Z),

    KB_DIGIT(0),
    KB_DIGIT(1),
    KB_DIGIT(2),
    KB_DIGIT(3),
    KB_DIGIT(4),
    KB_DIGIT(5),
    KB_DIGIT(6),
    KB_DIGIT(7),
    KB_DIGIT(8),
    KB_DIGIT(9),

    KB_FKEY(1),
    KB_FKEY(2),
    KB_FKEY(3),
    KB_FKEY(4),
    KB_FKEY(5),
    KB_FKEY(6),
    KB_FKEY(7),
    KB_FKEY(8),
    KB_FKEY(9),
    KB_FKEY(10),
    KB_FKEY(11),
    KB_FKEY(12),

    { "Esc", KEY_ESC, NULL },
    { "Escape", KEY_ESC, NULL },
    { "Enter", KEY_ENTER, NULL },
    { "Return", KEY_ENTER, NULL },
    { "Tab", KEY_TAB, NULL },
    { "Space", KEY_SPACE, NULL },
    { "Backspace", KEY_BACKSPACE, NULL },
    { "Delete", KEY_DELETE, NULL },
    { "Insert", KEY_INSERT, NULL },
    { "Home", KEY_HOME, NULL },
    { "End", KEY_END, NULL },
    { "PageUp", KEY_PAGEUP, NULL },
    { "PageDown", KEY_PAGEDOWN, NULL },
    { "Left", KEY_LEFT, NULL },
    { "Right", KEY_RIGHT, NULL },
    { "Up", KEY_UP, NULL },
    { "Down", KEY_DOWN, NULL },

    { "Equal", KEY_EQUAL, "=" },
    { "=", KEY_EQUAL, "=" },
    { "Minus", KEY_MINUS, "-" },
    { "-", KEY_MINUS, "-" },
    { "Slash", KEY_SLASH, "/" },
    { "/", KEY_SLASH, "/" },
    { "Backlash", KEY_BACKSLASH, "\\" },
    { "\\", KEY_BACKSLASH, "\\" },
    { "Comma", KEY_COMMA, "," },
    { "Period", KEY_DOT, "." },
    { ".", KEY_DOT, "." },
    { "Semicolon", KEY_SEMICOLON, ";" },
    { ";", KEY_SEMICOLON, ";" },
    { "Apostrophe", KEY_APOSTROPHE, "'" },
    { "'", KEY_APOSTROPHE, "'" },
    { "Grave", KEY_GRAVE, "`" },
    { "`", KEY_GRAVE, "`" },
    { "LeftBracket", KEY_LEFTBRACE, "[" },
    { "[", KEY_LEFTBRACE, "[" },
    { "RightBracket", KEY_RIGHTBRACE, "]" },
    { "]", KEY_RIGHTBRACE, "]" },

    { "KPPlus", KEY_KPPLUS, "KP+" },
    { "KPMinus", KEY_KPMINUS, "KP-" },
    { "KPEnter", KEY_KPENTER, "KPEnter" },
};

#define KEY_NAME_COUNT (sizeof(key_names)) / (sizeof(key_names[0]))

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';

    return s;
}

static int lookup_key(const char *name, uint32_t *code)
{
    for (size_t i = 0; i < KEY_NAME_COUNT; i++) {
        if (strcasecmp(name, key_names[i].name) == 0) {
            *code = key_names[i].code;
            return 0;
        }
    }

    return -1;
}

// parses a binding such as "CTRL+SHIFT+Z". '+' acts as the separator.
// That is why we spell the plus key as "SHIFT+EQUAL" (or "KPPLUS")
static int parse_bind(char *text, struct miru_bind *out)
{
    out->key = 0;
    out->mods = 0;

    bool have_key = false;
    char *cursor = text;

    while (cursor) {
        char *next = strchr(cursor, '+');
        if (next)
            *next++ = '\0';

        char *tok = trim(cursor);
        if (*tok == '\0') {
            MIRU_LOG("config: keybinds: empty key name in a binding (check for a stray '+')");
            return -1;
        }
        if (have_key) {
            MIRU_LOG("config: keybinds: '%s' must be the last part of a binding", tok);
            return -1;
        }

        if (strcasecmp(tok, "shift") == 0) {
            out->mods |= MIRU_MOD_SHIFT;
        } else if (strcasecmp(tok, "ctrl") == 0 || strcasecmp(tok, "control") == 0) {
            out->mods |= MIRU_MOD_CTRL;
        } else if (strcasecmp(tok, "alt") == 0 || strcasecmp(tok, "mod") == 0 || strcasecmp(tok, "super") == 0) {
            MIRU_LOG("config: keybinds: '%s' is not supported (Only SHIFT and CTRL)", tok);
            return -1;
        } else if (lookup_key(tok, &out->key) == 0) {
            have_key = true;
        } else {
            MIRU_LOG("config: keybinds: unknown key '%s'", tok);
            return -1;
        }

        cursor = next;
    }

    if (!have_key) {
        MIRU_LOG("config: keybinds: binding has no key");
        return -1;
    }

    return 0;
}

int keybinds_parse_list(const char *text, struct miru_bind *out, int max)
{
    char buf[256];
    if (!text || snprintf(buf, sizeof(buf), "%s", text) > (int)sizeof(buf)) {
        MIRU_LOG("config: keybinds: value is too long");
        return -1;
    }

    char *s = trim(buf);
    // "" or "none" unbinds the action
    if (*s == '\0' || strcasecmp(s, "none") == 0)
        return 0;

    int n = 0;
    char *cursor = s;

    while (cursor) {
        char *next = strchr(cursor, ',');
        if (next)
            *next++ = '\0';

        if (n >= max) {
            MIRU_LOG("config: keybinds: at most %d bindings per action", max);
            return -1;
        }

        if (parse_bind(trim(cursor), &out[n]) != 0) {
            return -1;
        }

        n++;
        cursor = next;
    }

    return n;
}

void keybind_defaults(struct miru_keybinds *kb)
{
    memset(kb, 0, sizeof(*kb));
    for (int a = 0; a < MIRU_ACT_COUNT; a++) {
        int n = keybinds_parse_list(actions[a].defaults, kb->binds[a], MIRU_KB_MAX_ALT);
        kb->count[a] = n > 0 ? (uint8_t)n : 0;
    }
}

void keybinds_load(struct miru_keybinds *kb, const struct toml_table *t)
{
    for (int a = 0; a < MIRU_ACT_COUNT; a++) {
        const char *value = toml_get_string(t, "keybinds", actions[a].name, NULL);
        if (!value)
            continue;

        struct miru_bind parsed[MIRU_KB_MAX_ALT];
        int n = keybinds_parse_list(value, parsed, MIRU_KB_MAX_ALT);
        if (n < 0) {
            MIRU_LOG("config: invalid keybinds.%s = \"%s\", keeping default", actions[a].name, value);
            continue;
        }

        memset(kb->binds[a], 0, sizeof(kb->binds[a]));
        memcpy(kb->binds[a], parsed, (size_t)n * sizeof(parsed[0]));
        kb->count[0] = (uint8_t)n;
    }
}

bool keybinds_match(const struct miru_keybinds *kb, enum miru_action action, uint32_t key, uint8_t mods)
{
    for (int i = 0; i < kb->count[action]; i++) {
        if (kb->binds[action][i].key == key && kb->binds[action][i].mods == mods)
            return true;
    }

    return false;
}

bool keybinds_equal(const struct miru_keybinds *a, const struct miru_keybinds *b)
{
    for (int act = 0; act < MIRU_ACT_COUNT; act++) {
        if (a->count[act] != b->count[act])
            return false;
        for (int i = 0; i < a->count[act]; i++) {
            if (a->binds[act][i].key != b->binds[act][i].key || a->binds[act][i].mods != b->binds[act][i].mods)
                return false;
        }
    }

    return true;
}

void keybinds_format(const struct miru_bind *b, char *out, size_t out_size)
{
    const char *label = NULL;
    for (size_t i = 0; i < KEY_NAME_COUNT; i++) {
        if (key_names[i].code == b->key) {
            label = key_names[i].label ? key_names[i].label : key_names[i].name;
            break;
        }
    }

    char fallback[16];
    if (!label) {
        snprintf(fallback, sizeof(fallback), "KEY%u", (unsigned)b->key);
        label = fallback;
    }

    snprintf(
        out,
        out_size,
        "%s%s%s",
        (b->mods & MIRU_MOD_CTRL) ? "CTRL+" : "",
        (b->mods & MIRU_MOD_SHIFT) ? "SHIFT+" : "",
        label
    );
}

// The name used for a key when it is written back to a config file
static const char *config_key_name(uint32_t code)
{
    for (size_t i = 0; i < KEY_NAME_COUNT; i++) {
        if (key_names[i].code == code && isalnum((unsigned char)key_names[i].name[0]))
            return key_names[i].name;
    }

    return "";
}

void keybinds_write(FILE *f, const struct miru_keybinds *kb)
{
    fputs("[keybinds]\n", f);

    for (int a = 0; a < MIRU_ACT_COUNT; a++) {
        fprintf(f, "%s = \"", actions[a].name);
        for (int i = 0; i < kb->count[a]; i++) {
            const struct miru_bind *b = &kb->binds[a][i];
            fprintf(
                f,
                "%s%s%s%s",
                i ? ", " : "",
                (b->mods & MIRU_MOD_CTRL) ? "CTRL+" : "",
                (b->mods & MIRU_MOD_SHIFT) ? "SHIFT+" : "",
                config_key_name(b->key)
            );
        }

        fputs("\"\n", f);
    }
}
