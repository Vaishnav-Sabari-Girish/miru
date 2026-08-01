#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include "toml.h"

struct toml_entry {
    char *section;
    char *key;
    char *value;
};

struct toml_table {
    struct toml_entry *entries;
    size_t count;
    size_t capacity;
};

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static int add_entry(struct toml_table *t, const char *section, const char *key, const char *value)
{
    if (t->count == t->capacity) {
        size_t new_cap = t->capacity ? t->capacity * 2 : 16;
        struct toml_entry *new_entries = realloc(t->entries, new_cap * sizeof(*new_entries));
        if (!new_entries)
            return -1;
        t->entries = new_entries;
        t->capacity = new_cap;
    }
    struct toml_entry *e = &t->entries[t->count];
    e->section = strdup(section);
    e->key = strdup(key);
    e->value = strdup(value);
    if (!e->section || !e->key || !e->value) {
        free(e->section);
        free(e->key);
        free(e->value);
        return -1;
    }
    t->count++;
    return 0;
}

// scans for an unquoted '#' and truncates there. tracks both single-quoted
// (literal, no escapes) and double-quoted (escapable) string spans so a '#'
// or unbalanced quote inside either kind of string doesn't get misread
static void strip_trailing_comment(char *value)
{
    int i = 0;
    while (value[i]) {
        char c = value[i];
        if (c == '\'') {
            i++;
            while (value[i] && value[i] != '\'')
                i++;
            if (value[i] == '\'')
                i++;
            continue;
        }
        if (c == '"') {
            i++;
            while (value[i] && value[i] != '"') {
                if (value[i] == '\\' && value[i + 1])
                    i++; // skip escaped char, don't let it close the string early
                i++;
            }
            if (value[i] == '"')
                i++;
            continue;
        }
        if (c == '#') {
            value[i] = '\0';
            return;
        }
        i++;
    }
}

// strips one layer of quoting. single-quoted values are TOML "literal
// strings" — stripped verbatim, no escape processing. double-quoted values
// get \" and \\ unescaped (the two escapes that actually show up in
// practice for a config file; full TOML escape support is out of scope
// for this minimal parser)
static char *unquote(char *value)
{
    size_t len = strlen(value);

    if (len >= 2 && value[0] == '\'' && value[len - 1] == '\'') {
        value[len - 1] = '\0';
        return value + 1;
    }

    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        value[len - 1] = '\0';
        char *src = value + 1;
        char *dst = src;
        while (*src) {
            if (*src == '\\' && (src[1] == '"' || src[1] == '\\')) {
                *dst++ = src[1];
                src += 2;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        return value + 1;
    }

    return value;
}

struct toml_table *toml_parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }

    struct toml_table *t = calloc(1, sizeof(*t));
    if (!t) {
        fclose(f);
        return NULL;
    }

    char line[1024];
    char current_section[256] = "";

    while (fgets(line, sizeof(line), f)) {
        char *l = trim(line);
        if (*l == '\0' || *l == '#') {
            continue;
        }

        if (l[0] == '[') {
            // strip a trailing "# comment" and re-trim before validating the
            // header shape, so "[zoom] # comment" and "[ zoom ]" both parse
            strip_trailing_comment(l);
            char *header = trim(l);
            size_t hlen = strlen(header);
            if (hlen >= 2 && header[hlen - 1] == ']') {
                header[hlen - 1] = '\0';
                char *inner = trim(header + 1);
                snprintf(current_section, sizeof(current_section), "%s", inner);
            } else {
                current_section[0] = '\0';
            }
            // malformed header (no closing bracket left after stripping) — skip silently
            continue;
        }

        char *eq = strchr(l, '=');
        if (!eq) {
            continue; // malformed line, skip silently rather than fail the whole file
        }

        *eq = '\0';
        char *key = trim(l);
        char *value = trim(eq + 1);

        strip_trailing_comment(value);
        value = trim(value);
        value = unquote(value);

        if (add_entry(t, current_section, key, value) != 0) {
            toml_free(t);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);
    return t;
}

void toml_free(struct toml_table *t)
{
    if (!t) {
        return;
    }
    for (size_t i = 0; i < t->count; i++) {
        free(t->entries[i].section);
        free(t->entries[i].key);
        free(t->entries[i].value);
    }
    free(t->entries);
    free(t);
}

static const char *find_raw(const struct toml_table *t, const char *section, const char *key)
{
    if (!t) {
        return NULL;
    }
    for (size_t i = 0; i < t->count; i++) {
        if (strcmp(t->entries[i].section, section) == 0 && strcmp(t->entries[i].key, key) == 0) {
            return t->entries[i].value;
        }
    }
    return NULL;
}

const char *toml_get_string(const struct toml_table *t, const char *section, const char *key, const char *def)
{
    const char *v = find_raw(t, section, key);
    return v ? v : def;
}

long toml_get_int(const struct toml_table *t, const char *section, const char *key, long def)
{
    const char *v = find_raw(t, section, key);
    if (!v || *v == '\0') {
        return def;
    }
    errno = 0;
    char *end = NULL;
    long result = strtol(v, &end, 10);
    if (end == v) {
        return def; // no digits consumed at all
    }
    while (*end && isspace((unsigned char)*end))
        end++; // allow trailing whitespace
    if (*end != '\0') {
        return def; // trailing garbage after the number, e.g. "3abc" — reject rather than silently truncate
    }
    if (errno == ERANGE) {
        return def; // overflowed the type
    }
    return result;
}

double toml_get_double(const struct toml_table *t, const char *section, const char *key, double def)
{
    const char *v = find_raw(t, section, key);
    if (!v || *v == '\0') {
        return def;
    }
    errno = 0;
    char *end = NULL;
    double result = strtod(v, &end);
    if (end == v) {
        return def;
    }
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end != '\0') {
        return def;
    }
    if (errno == ERANGE) {
        return def;
    }
    if (!isfinite(result)) {
        return def; // strtod happily parses "nan"/"inf" text — reject those for config values
    }
    return result;
}

bool toml_get_bool(const struct toml_table *t, const char *section, const char *key, bool def)
{
    const char *v = find_raw(t, section, key);
    if (!v) {
        return def;
    }
    if (strcmp(v, "true") == 0) {
        return true;
    }
    if (strcmp(v, "false") == 0) {
        return false;
    }
    return def;
}
