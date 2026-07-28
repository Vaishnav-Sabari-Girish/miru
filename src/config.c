#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "toml.h"
#include <unistd.h>

static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    return -1;
}

static int create_default_config(const char *config_dir, const char *config_path)
{
    if (ensure_dir(config_dir) != 0) {
        fprintf(stderr, "config: failed to create  directory %s: %s\n", config_dir, strerror(errno));
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

    static const char default_config[] = "[zoom]\n"
                                         "factor = 3.0\n"
                                         "increment = 0.5\n"
                                         "max_factor = 6.0\n"
                                         "smooth = true\n"
                                         "\n"
                                         "[spotlight]\n"
                                         "radius = 250\n"
                                         "dim = 0.65\n"
                                         "softness = 20\n"
                                         "\n"
                                         "[general]\n"
                                         "show_cursor = true\n";

    fputs(default_config, f);
    fclose(f);

    fprintf(stderr, "config: created default config at %s\n", config_path);

    return 0;
}

static int get_config_path(char *dir, size_t dir_size, char *file, size_t file_size)
{
    const char *base = getenv("XDG_CONFIG_HOME");

    if (!base || !*base) {
        const char *home = getenv("HOME");

        if (!home || !*home) {
            return -1;
        }

        snprintf(dir, dir_size, "%s/.config/miru", home);
    } else {
        snprintf(dir, dir_size, "%s/miru", base);
    }

    snprintf(file, file_size, "%s/config.toml", dir);

    return 0;
}

void config_load(struct miru_config *out)
{
    // Built-in defaults
    out->zoom_factor = 2.0;
    out->zoom_increment = 0.25;
    out->zoom_max_factor = 10.0;
    out->zoom_smooth = 0;

    out->spotlight_radius = 250;
    out->spotlight_dim = 0.65;
    out->spotlight_softness = 20;

    out->show_cursor = 1;

    char config_dir[512];
    char config_path[512];

    if (get_config_path(config_dir, sizeof(config_dir), config_path, sizeof(config_path)) != 0) {
        fprintf(stderr, "config: unable to determine config directory\n");
        return;
    }

    if (access(config_path, F_OK) != 0) {
        if (errno == ENOENT) {
            create_default_config(config_dir, config_path);
        }
    }

    struct toml_table *t = toml_parse_file(config_path);
    if (!t)
        return;

    out->zoom_factor = toml_get_double(t, "zoom", "factor", out->zoom_factor);

    out->zoom_increment = toml_get_double(t, "zoom", "increment", out->zoom_increment);

    out->zoom_max_factor = toml_get_double(t, "zoom", "max_factor", out->zoom_max_factor);

    out->zoom_smooth = toml_get_bool(t, "zoom", "smooth", out->zoom_smooth);

    out->spotlight_radius = toml_get_int(t, "spotlight", "radius", out->spotlight_radius);

    out->spotlight_dim = toml_get_double(t, "spotlight", "dim", out->spotlight_dim);

    out->spotlight_softness = toml_get_int(t, "spotlight", "softness", out->spotlight_softness);

    out->show_cursor = toml_get_bool(t, "general", "show_cursor", out->show_cursor);

    toml_free(t);
}
