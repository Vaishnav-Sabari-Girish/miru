#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "toml.h"

// mkdir -p equivalent: mkdir() only ever creates the final path component,
// so a fresh install where ~/.config itself doesn't exist yet would
// otherwise fail here silently
static int ensure_dir(const char *path)
{
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        return -1;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int create_default_config(const char *config_dir, const char *config_path)
{
    if (ensure_dir(config_dir) != 0) {
        fprintf(stderr, "config: failed to create directory %s: %s\n", config_dir, strerror(errno));
        return -1;
    }

    FILE *f = fopen(config_path, "wx");
    if (!f) {
        if (errno == EEXIST) {
            return 0;
        }
        fprintf(stderr, "config: failed to create %s: %s\n", config_path, strerror(errno));
        return -1;
    }

    // deliberately identical to config_load's built-in defaults below —
    // a fresh install must behave exactly like "no config file" until the
    // user actually edits something, not silently adopt different values
    static const char default_config[] = "[zoom]\n"
                                         "factor = 2.0\n"
                                         "increment = 0.25\n"
                                         "max_factor = 10.0\n"
                                         "smooth = false\n"
                                         "\n"
                                         "[spotlight]\n"
                                         "radius = 250\n"
                                         "dim = 0.65\n"
                                         "softness = 20\n"
                                         "\n"
                                         "[general]\n"
                                         "show_cursor = true\n";

    if (fputs(default_config, f) == EOF) {
        fclose(f);
        unlink(config_path);
        fprintf(stderr, "config: failed writing default config to %s\n", config_path);
        return -1;
    }
    if (fclose(f) != 0) {
        unlink(config_path); // catches a flush failure (e.g. disk full) that fputs alone wouldn't see
        fprintf(stderr, "config: failed closing default config %s: %s\n", config_path, strerror(errno));
        return -1;
    }

    fprintf(stderr, "config: created default config at %s\n", config_path);
    return 0;
}

static int get_config_path(char *dir, size_t dir_size, char *file, size_t file_size)
{
    const char *base = getenv("XDG_CONFIG_HOME");
    int n;

    if (!base || !*base) {
        const char *home = getenv("HOME");
        if (!home || !*home) {
            return -1;
        }
        n = snprintf(dir, dir_size, "%s/.config/miru", home);
    } else {
        n = snprintf(dir, dir_size, "%s/miru", base);
    }
    if (n < 0 || (size_t)n >= dir_size) {
        return -1; // truncated — a very long HOME/XDG_CONFIG_HOME must fail loudly, not silently point elsewhere
    }

    n = snprintf(file, file_size, "%s/config.toml", dir);
    if (n < 0 || (size_t)n >= file_size) {
        return -1;
    }

    return 0;
}

int config_get_watch_paths(char *dir, size_t dir_size, char *filename, size_t filename_size)
{
    char full_path[512];
    if (get_config_path(dir, dir_size, full_path, sizeof(full_path)) != 0) {
        return -1;
    }

    int n = snprintf(filename, filename_size, "config.toml");
    if (n < 0 || (size_t)n >= filename_size) {
        return -1;
    }

    return 0;
}

// clamps/rejects anything a user could put in the file that would otherwise
// reach layer_surface/input as NaN, negative, or otherwise nonsensical
static void sanitize_config(struct miru_config *c)
{
    if (!isfinite(c->zoom_factor) || c->zoom_factor < 1.0) {
        fprintf(stderr, "config: invalid zoom.factor, falling back to default\n");
        c->zoom_factor = 2.0;
    }
    if (!isfinite(c->zoom_increment) || c->zoom_increment <= 0.0) {
        fprintf(stderr, "config: invalid zoom.increment (must be positive), falling back to default\n");
        c->zoom_increment = 0.25;
    }
    if (!isfinite(c->zoom_max_factor) || c->zoom_max_factor < 1.0) {
        fprintf(stderr, "config: invalid zoom.max_factor, falling back to default\n");
        c->zoom_max_factor = 10.0;
    }
    if (c->zoom_factor > c->zoom_max_factor) {
        fprintf(stderr, "config: zoom.factor exceeds zoom.max_factor, clamping\n");
        c->zoom_factor = c->zoom_max_factor;
    }
}

void config_load(struct miru_config *out)
{
    // Built-in defaults
    out->zoom_factor = 2.0;
    out->zoom_increment = 0.25;
    out->zoom_max_factor = 10.0;
    out->zoom_smooth = false;

    // [spotlight]/[general] below are parsed and stored but not yet
    // consumed anywhere — Spotlight mode and cursor rendering aren't built
    // yet. Kept here so the config file's shape is stable once they are,
    // rather than adding these fields twice later.
    out->spotlight_radius = 250;
    out->spotlight_dim = 0.65;
    out->spotlight_softness = 20;

    out->show_cursor = true;

    char config_dir[512];
    char config_path[512];

    if (get_config_path(config_dir, sizeof(config_dir), config_path, sizeof(config_path)) != 0) {
        fprintf(stderr, "config: unable to determine config directory\n");
        return;
    }

    if (access(config_path, F_OK) != 0 && errno == ENOENT) {
        create_default_config(config_dir, config_path);
    }

    struct toml_table *t = toml_parse_file(config_path);
    if (!t) {
        sanitize_config(out); // defensive, built-ins above are already valid, but cheap
        return;
    }

    out->zoom_factor = toml_get_double(t, "zoom", "factor", out->zoom_factor);
    out->zoom_increment = toml_get_double(t, "zoom", "increment", out->zoom_increment);
    out->zoom_max_factor = toml_get_double(t, "zoom", "max_factor", out->zoom_max_factor);
    out->zoom_smooth = toml_get_bool(t, "zoom", "smooth", out->zoom_smooth);

    out->spotlight_radius = toml_get_int(t, "spotlight", "radius", out->spotlight_radius);
    out->spotlight_dim = toml_get_double(t, "spotlight", "dim", out->spotlight_dim);
    out->spotlight_softness = toml_get_int(t, "spotlight", "softness", out->spotlight_softness);

    out->show_cursor = toml_get_bool(t, "general", "show_cursor", out->show_cursor);

    toml_free(t);

    sanitize_config(out);

    fprintf(
        stderr,
        "config: loaded from %s (zoom factor=%.2f increment=%.2f max=%.2f)\n",
        config_path,
        out->zoom_factor,
        out->zoom_increment,
        out->zoom_max_factor
    );
}
