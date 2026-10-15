#include "export.h"
#include "debug.h"
#include <GLES2/gl2.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int ensure_dir(const char *path)
{
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s", path);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return -1;

    size_t len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int write_bmp_bgr(const char *path, int width, int height, const uint8_t *rgba_bottom_up)
{
    int row_stride = (width * 3 + 3) & ~3;
    size_t pixel_bytes = (size_t)row_stride * (size_t)height;
    uint8_t *bgr = calloc(1, pixel_bytes);

    if (!bgr)
        return -1;

    for (int y = 0; y < height; y++) {
        const uint8_t *src = rgba_bottom_up + (size_t)y * (size_t)width * 4;
        uint8_t *dst = bgr + (size_t)y * (size_t)row_stride;
        for (int x = 0; x < width; x++) {
            dst[x * 3 + 0] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 0];
        }
    }

    uint8_t header[54];
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';

    uint32_t file_size = 54u + (uint32_t)pixel_bytes;

    memcpy(header + 2, &file_size, 4);
    header[10] = 54;
    header[14] = 40;

    memcpy(header + 18, &width, 4);
    memcpy(header + 22, &height, 4);

    header[26] = 1;
    header[28] = 24;

    FILE *f = fopen(path, "wb");

    if (!f) {
        free(bgr);
        return -1;
    }

    if (fwrite(header, 1, 54, f) != 54 || fwrite(bgr, 1, pixel_bytes, f) != pixel_bytes) {
        fclose(f);
        free(bgr);
        return -1;
    }

    fclose(f);
    free(bgr);
    return 0;
}

static int write_png_rbga(const char *path, int width, int height, const uint8_t *rgba_bottom_up)
{
    FILE *f = fopen(path, "wb");

    if (!f)
        return -1;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(f);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(f);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        unlink(path);
        return -1;
    }

    png_init_io(png, f);
    png_set_IHDR(
        png,
        info,
        (png_uint_32)width,
        (png_uint_32)height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    png_bytep *rows = (png_bytep *)malloc((size_t)height * sizeof(png_bytep));
    if (!rows) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        unlink(path);
        return -1;
    }

    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y;
        rows[y] = (png_bytep)(rgba_bottom_up + (size_t)src_y * (size_t)width * 4);
    }

    png_write_image(png, rows);
    png_write_end(png, NULL);

    free((void *)rows);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return 0;
}

int miru_export_framebuffer(int width, int height, int format)
{
    if (width <= 0 || height <= 0)
        return -1;

    size_t nbytes = (size_t)width * (size_t)height * 4;

    uint8_t *rgba = malloc(nbytes);
    if (!rgba)
        return -1;

    while (glGetError() != GL_NO_ERROR) {
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        MIRU_LOG("export: glReadPixels failed glerr=0x%x", (unsigned)err);
        free(rgba);
        return -1;
    }

    const char *home = getenv("HOME");
    if (!home)
        home = ".";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/Pictures/miru", home);
    if (ensure_dir(dir) != 0) {
        MIRU_LOG("export: Cannot create %s: %s", dir, strerror(errno));
        free(rgba);
        return -1;
    }

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    const char *ext = (format == MIRU_EXPORT_PNG) ? "png" : "bmp";

    char path[640];

    snprintf(
        path,
        sizeof(path),
        "%s/miru-%04d%02d%02d-%02d%02d%02d.%s",
        dir,
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        ext
    );

    int rc = (format == MIRU_EXPORT_PNG) ? write_png_rbga(path, width, height, rgba) :
                                           write_bmp_bgr(path, width, height, rgba);

    free(rgba);
    if (rc != 0) {
        MIRU_LOG("export: write failed to %s: %s", path, strerror(errno));
        return -1;
    }

    MIRU_LOG("export: wrote %s", path);
    return 0;
}
