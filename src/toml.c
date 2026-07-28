#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
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
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

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

        if (!new_entries) {
            return -1;
        }

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

static void strip_trailing_comment(char *value)
{
    int in_quotes = 0;
    for (char *p = value; *p; p++) {
        if (*p == '"') {
            in_quotes = !in_quotes;
        } else if (*p == '#' && !in_quotes) {
            *p = '\0';
            break;
        }
    }
}

static char *unquote(char *value)
{
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        value[len - 1] = '\0';
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

        size_t len = strlen(l);

        if (l[0] == '[' && l[len - 1] == ']') {
            l[len - 1] = '\0';
            snprintf(current_section, sizeof(current_section), "%s", l + 1);
            continue;
        }

        char *eq = strchr(l, '=');

        if (!eq) {
            continue;
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

long toml_get_int(const struct toml_table *table, const char *section, const char *key, long default_value)
{
    const char *v = find_raw(table, section, key);

    if (!v) {
        return default_value;
    }

    char *end = NULL;
    long result = strtol(v, &end, 10);
    return (end != v) ? result : default_value;
}

double toml_get_double(const struct toml_table *table, const char *section, const char *key, double default_value)
{
    const char *v = find_raw(table, section, key);

    if (!v) {
        return default_value;
    }

    char *end = NULL;
    double result = strtod(v, &end);
    return (end != v) ? result : default_value;
}

int toml_get_bool(const struct toml_table *table, const char *section, const char *key, int default_value)
{
    const char *v = find_raw(table, section, key);

    if (!v) {
        return default_value;
    }

    if (strcmp(v, "true") == 0) {
        return 1;
    }
    if (strcmp(v, "false") == 0) {
        return 0;
    }

    return default_value;
}
