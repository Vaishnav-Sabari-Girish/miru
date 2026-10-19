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
#include "debug.h"
#include "export.h"

static int parse_color_filter(const char *s)
{
    if (!s)
        return 0;

    if (strcmp(s, "grayscale") == 0 || strcmp(s, "gray") == 0 || strcmp(s, "grey") == 0)
        return 1;

    if (strcmp(s, "invert") == 0 || strcmp(s, "inverted") == 0 || strcmp(s, "negative") == 0)
        return 2;

    if (strcmp(s, "contrast") == 0 || strcmp(s, "high-contrast") == 0)
        return 3;

    return 0;
}

static int parse_export_format(const char *s)
{
    if (!s)
        return MIRU_EXPORT_PNG;

    if (strcmp(s, "bmp") == 0)
        return MIRU_EXPORT_BMP;

    if (strcmp(s, "png") == 0)
        return MIRU_EXPORT_PNG;

    if (strcmp(s, "jpeg") == 0 || strcmp(s, "jpg") == 0) {
        MIRU_LOG("config: export.%s format is not supported yet, using png", s);
        return MIRU_EXPORT_PNG;
    }

    MIRU_LOG("config: unknown export format '%s', using png", s);
    return MIRU_EXPORT_PNG;
}

static int parse_hex_rgb(const char *s, float *r, float *g, float *b)
{
    if (!s || !*s)
        return -1;
    if (s[0] == '#')
        s++;

    char *end = NULL;
    unsigned long v = strtoul(s, &end, 16);

    if (end == s || *end != '\0' || v > 0xffffffUL)
        return -1;

    *r = ((v >> 16) & 0xff) / 255.0f;
    *g = ((v >> 8) & 0xff) / 255.0f;
    *b = (v & 0xff) / 255.0f;

    return 0;
}

static int parse_annotate_default(const char *s)
{
    if (!s)
        return 0;

    if (strcmp(s, "rect") == 0 || strcmp(s, "rectangle") == 0)
        return 1;
    if (strcmp(s, "text") == 0)
        return 2;
    if (strcmp(s, "line") == 0)
        return 3;
    if (strcmp(s, "ellipse") == 0)
        return 4;

    return 0; // arrow
}

static int parse_loupe_shape(const char *s)
{
    if (!s)
        return 0;

    if (strcmp(s, "ellipse") == 0)
        return 1;
    if (strcmp(s, "circle") == 0)
        return 2;

    return 0;
}

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
        // fprintf(stderr, "config: failed to create directory %s: %s\n", config_dir, strerror(errno));
        MIRU_LOG("config: failed to create directory %s: %s", config_dir, strerror(errno));
        return -1;
    }

    FILE *f = fopen(config_path, "wx");
    if (!f) {
        if (errno == EEXIST) {
            return 0;
        }
        // fprintf(stderr, "config: failed to create %s: %s\n", config_path, strerror(errno));
        MIRU_LOG("config: failed to create %s: %s", config_path, strerror(errno));
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
                                         "upscale = \"bicubic\"\n"
                                         "\n"
                                         "[spotlight]\n"
                                         "radius = 250\n"
                                         "dim = 0.65\n"
                                         "softness = 20\n"
                                         "animation_speed = 14.0\n"
                                         "radius_step = 20.0\n"
                                         "\n"
                                         "[general]\n"
                                         "show_cursor = true\n"
                                         "\n"
                                         "[annotate]\n"
                                         "default = \"rect\"\n"
                                         "thickness = 4.0\n"
                                         "color = \"ff3333\"\n"
                                         "text_color = \"ffd933\"\n"
                                         "text_size = 1.0\n"
                                         "\n"
                                         "[loupe]\n"
                                         "shape = \"rect\"\n"
                                         "\n"
                                         "[export]\n"
                                         "format = \"png\"\n"
                                         "\n"
                                         "[filter]\n"
                                         "default = \"none\"\n";

    if (fputs(default_config, f) == EOF) {
        fclose(f);
        unlink(config_path);
        // fprintf(stderr, "config: failed writing default config to %s\n", config_path);
        MIRU_LOG("config: failed writing default config to %s", config_path);
        return -1;
    }
    if (fclose(f) != 0) {
        unlink(config_path); // catches a flush failure (e.g. disk full) that fputs alone wouldn't see
        // fprintf(stderr, "config: failed closing default config %s: %s\n", config_path, strerror(errno));
        MIRU_LOG("config: failed closing default config %s: %s", config_path, strerror(errno));
        return -1;
    }

    // fprintf(stderr, "config: created default config at %s\n", config_path);
    MIRU_LOG("config: created default config at %s", config_path);
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
        // fprintf(stderr, "config: invalid zoom.factor, falling back to default\n");
        MIRU_LOG("config: invalid zoom.factor, falling back to default");
        c->zoom_factor = 2.0;
    }
    if (!isfinite(c->zoom_increment) || c->zoom_increment <= 0.0) {
        // fprintf(stderr, "config: invalid zoom.increment (must be positive), falling back to default\n");
        MIRU_LOG("config: invalid zoom.increment (must be positive), falling back to default");
        c->zoom_increment = 0.25;
    }
    if (!isfinite(c->zoom_max_factor) || c->zoom_max_factor < 1.0) {
        // fprintf(stderr, "config: invalid zoom.max_factor, falling back to default\n");
        MIRU_LOG("config: invalid zoom.max_factor, falling back to default");
        c->zoom_max_factor = 10.0;
    }
    if (c->zoom_factor > c->zoom_max_factor) {
        // fprintf(stderr, "config: zoom.factor exceeds zoom.max_factor, clamping\n");
        MIRU_LOG("config: zoom.factor exceeds zoom.max_factor, clamping");
        c->zoom_factor = c->zoom_max_factor;
    }

    if (!isfinite(c->spotlight_dim) || c->spotlight_dim < 0.0) {
        // fprintf(stderr, "config: invalid spotlight.dim, clamping to 0.0\n");
        MIRU_LOG("config: invalid spotlight.dim, clamping to 0.0");
        c->spotlight_dim = 0.0;
    } else if (c->spotlight_dim > 1.0) {
        // fprintf(stderr, "config: spotlight.dim exceeds 1.0, clamping to 1.0\n");
        MIRU_LOG("config: spotlight.dim exceeds 1.0, clamping to 1.0");
        c->spotlight_dim = 1.0;
    }

    if (c->spotlight_radius < 0) {
        // fprintf(stderr, "config: invalid spotlight.radius, falling back to default\n");
        MIRU_LOG("config: invalid spotlight.radius, falling back to default");
        c->spotlight_radius = 250;
    }

    if (c->spotlight_softness < 0) {
        // fprintf(stderr, "config: invalid spotlight.softness, falling back to default\n");
        MIRU_LOG("config: invalid spotlight.softness, falling back to default");
        c->spotlight_softness = 20;
    }

    if (!isfinite(c->spotlight_animation_speed) || c->spotlight_animation_speed <= 0.0) {
        // fprintf(stderr, "config: invalid spotlight.animation_speed (must be positive). falling back to default\n");
        MIRU_LOG("config: invalid spotlight.animation_speed (must be positive). falling back to default");
        c->spotlight_animation_speed = 14.0;
    } else if (c->spotlight_animation_speed > 60.0) {
        // fprintf(stderr, "config: spotlight.animation_speed exceeds 60.0. clamping\n");
        MIRU_LOG("config: spotlight.animation_speed exceeds 60.0. clamping");
        c->spotlight_animation_speed = 60.0;
    }

    if (!isfinite(c->spotlight_radius_step) || c->spotlight_radius_step <= 0.0) {
        // fprintf(stderr, "config: invalid spotlight.radius_step (must be positive), falling back to defaults\n");
        MIRU_LOG("config: invalid spotlight.radius_step (must be positive), falling back to defaults");
        c->spotlight_radius_step = 20.0;
    } else if (c->spotlight_radius_step > 500.0) {
        // fprintf(stderr, "config: spotlight.radius_step exceeds 500.0, clamping\n");
        MIRU_LOG("config: spotlight.radius_step exceeds 500.0, clamping");
        c->spotlight_radius_step = 500.0;
    }

    if (!isfinite(c->zoom_animation_speed) || c->zoom_animation_speed <= 0)
        c->zoom_animation_speed = 14.0;

    if (c->annotate_default < 0 || c->annotate_default > 4)
        c->annotate_default = 0;

    if (!isfinite(c->annotate_thickness) || c->annotate_thickness < 0.0)
        c->annotate_thickness = 4.0;
    else if (c->annotate_thickness > 64.0)
        c->annotate_thickness = 64.0;

    if (!isfinite(c->annotate_text_size) || c->annotate_text_size < 0.25)
        c->annotate_text_size = 1.0;
    else if (c->annotate_text_size > 8.0)
        c->annotate_text_size = 8.0;

    if (c->loupe_shape < 0 || c->loupe_shape > 2)
        c->loupe_shape = 0;

    if (c->export_format != MIRU_EXPORT_PNG && c->export_format != MIRU_EXPORT_BMP)
        c->export_format = MIRU_EXPORT_PNG;

    if (c->color_filter < 0 || c->color_filter > 3)
        c->color_filter = 0;
}

void config_load(struct miru_config *out)
{
    // Built-in defaults
    out->zoom_factor = 2.0;
    out->zoom_increment = 0.25;
    out->zoom_max_factor = 10.0;
    out->zoom_smooth = true;
    out->zoom_animation_speed = 14.0;
    out->zoom_upscale = 2;

    out->spotlight_radius = 250;
    out->spotlight_dim = 0.65;
    out->spotlight_softness = 20;
    out->spotlight_animation_speed = 14.0;
    out->spotlight_radius_step = 20.0;

    out->show_cursor = true;

    out->annotate_default = 1; // rect
    out->annotate_thickness = 4.0f;
    out->annotate_color_r = 1.0f;
    out->annotate_color_g = 0.2f;
    out->annotate_color_b = 0.2f;
    out->annotate_text_r = 1.0f;
    out->annotate_text_g = 0.85f;
    out->annotate_text_b = 0.2f;
    out->annotate_text_size = 1.0;

    out->loupe_shape = 0;

    out->export_format = MIRU_EXPORT_PNG;

    out->color_filter = 0;

    char config_dir[512];
    char config_path[512];

    if (get_config_path(config_dir, sizeof(config_dir), config_path, sizeof(config_path)) != 0) {
        // fprintf(stderr, "config: unable to determine config directory\n");
        MIRU_LOG("config: unable to determine config directory");
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
    out->zoom_animation_speed = toml_get_double(t, "zoom", "animation_speed", out->zoom_animation_speed);

    {
        const char *up = toml_get_string(t, "zoom", "upscale", "bicubic");
        if (strcmp(up, "nearest") == 0)
            out->zoom_upscale = 0;
        else if (strcmp(up, "linear") == 0)
            out->zoom_upscale = 1;
        else
            out->zoom_upscale = 2;

        if (out->zoom_upscale < 0 || out->zoom_upscale > 2)
            out->zoom_upscale = 2;
    }

    out->spotlight_radius = toml_get_int(t, "spotlight", "radius", out->spotlight_radius);
    out->spotlight_dim = toml_get_double(t, "spotlight", "dim", out->spotlight_dim);
    out->spotlight_softness = toml_get_int(t, "spotlight", "softness", out->spotlight_softness);
    out->spotlight_animation_speed = toml_get_double(t, "spotlight", "animation_speed", out->spotlight_animation_speed);
    out->spotlight_radius_step = toml_get_double(t, "spotlight", "radius_step", out->spotlight_radius_step);

    out->show_cursor = toml_get_bool(t, "general", "show_cursor", out->show_cursor);

    out->annotate_default = parse_annotate_default(toml_get_string(t, "annotate", "default", "arrow"));
    out->annotate_thickness = toml_get_double(t, "annotate", "thickness", out->annotate_thickness);
    out->annotate_text_size = toml_get_double(t, "annotate", "text_size", out->annotate_text_size);

    {
        float r, g, b;

        if (parse_hex_rgb(toml_get_string(t, "annotate", "color", "ff3333"), &r, &g, &b) == 0) {
            out->annotate_color_r = r;
            out->annotate_color_g = g;
            out->annotate_color_b = b;
        }

        if (parse_hex_rgb(toml_get_string(t, "annotate", "text_color", "ffd933"), &r, &g, &b) == 0) {
            out->annotate_text_r = r;
            out->annotate_text_g = g;
            out->annotate_text_b = b;
        }
    }

    out->loupe_shape = parse_loupe_shape(toml_get_string(t, "loupe", "shape", "rect"));

    out->export_format = parse_export_format(toml_get_string(t, "export", "format", "png"));

    out->color_filter = parse_color_filter(toml_get_string(t, "filter", "default", "none"));

    toml_free(t);

    sanitize_config(out);

    // fprintf(
    //     stderr,
    //     "config: loaded from %s (zoom factor=%.2f increment=%.2f max=%.2f)\n",
    //     config_path,
    //     out->zoom_factor,
    //     out->zoom_increment,
    //     out->zoom_max_factor
    // );
    MIRU_LOG(
        "config: loaded from %s (zoom factor=%.2f increment=%.2f max=%.2f)\n",
        config_path,
        out->zoom_factor,
        out->zoom_increment,
        out->zoom_max_factor
    );
}
