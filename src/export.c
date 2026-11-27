#include "export.h"
#include "debug.h"
#include <GLES2/gl2.h>
#include <errno.h>
#include <libnotify/notification.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <webp/encode.h>
#include <libnotify/notify.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void notify_export(const char *path)
{
    if (!path || !*path)
        return;

    static int inited = 0;

    if (!inited) {
        if (!notify_init("miru"))
            return;
        inited = 1;
    }

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    NotifyNotification *n = notify_notification_new("Miru Export", name, "image-x-generic");
    if (!n)
        return;

    notify_notification_set_timeout(n, 4000);
    notify_notification_show(n, NULL);
    g_object_unref(G_OBJECT(n));
}

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

#define MIRU_EXPORT_JPEG_QUALITY 90
#define MIRU_EXPORT_WEBP_QUALITY 90.0f

static const char *export_ext(int format)
{
    switch (format) {
    case MIRU_EXPORT_BMP:
        return "bmp";
    case MIRU_EXPORT_JPEG:
        return "jpeg";
    case MIRU_EXPORT_WEBP:
        return "webp";
    default:
        return "png";
    }
}

struct jpeg_error {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
};

static void jpeg_error_exit(j_common_ptr cinfo)
{
    struct jpeg_error *err = (struct jpeg_error *)cinfo->err;
    longjmp(err->jump, 1);
}

static int write_jpeg_rgb(const char *path, int width, int height, const uint8_t *rgba_bottom_up)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;

    JSAMPLE *row = malloc((size_t)width * 3);
    if (!row) {
        fclose(f);
        unlink(path);
        return -1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error jerr;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    jpeg_create_compress(&cinfo);

    if (setjmp(jerr.jump)) {
        jpeg_destroy_compress(&cinfo);
        free(row);
        fclose(f);
        unlink(path);
        return -1;
    }

    jpeg_stdio_dest(&cinfo, f);
    cinfo.image_width = (JDIMENSION)width;
    cinfo.image_height = (JDIMENSION)height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, MIRU_EXPORT_JPEG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        int src_y = height - 1 - (int)cinfo.next_scanline;
        const uint8_t *src = rgba_bottom_up + (size_t)src_y * (size_t)width * 4;
        for (int x = 0; x < width; x++) {
            row[x * 3 + 0] = src[x * 4 + 0];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 2];
        }
        JSAMPROW row_ptr = row;
        jpeg_write_scanlines(&cinfo, &row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    free(row);
    if (fclose(f) != 0) {
        unlink(path);
        return -1;
    }
    return 0;
}

static int write_webp_rgb(const char *path, int width, int height, const uint8_t *rgba_bottom_up)
{
    size_t stride = (size_t)width * 4;
    uint8_t *flipped = malloc(stride * (size_t)height);
    if (!flipped)
        return -1;

    for (int y = 0; y < height; y++)
        memcpy(flipped + (size_t)y * stride, rgba_bottom_up + (size_t)(height - 1 - y) * stride, stride);

    uint8_t *encoded = NULL;
    size_t size = WebPEncodeRGBA(flipped, width, height, (int)stride, MIRU_EXPORT_WEBP_QUALITY, &encoded);
    free(flipped);
    if (size == 0 || !encoded)
        return -1;

    FILE *f = fopen(path, "wb");
    if (!f) {
        WebPFree(encoded);
        return -1;
    }

    int rc = fwrite(encoded, 1, size, f) == size ? 0 : -1;
    WebPFree(encoded);
    if (fclose(f) != 0)
        rc = -1;
    if (rc != 0)
        unlink(path);
    return rc;
}

static void mask_outside_ellipse(
    uint8_t *rgba,
    int x,
    int y,
    int width,
    int height,
    float cx,
    float cy,
    float rx,
    float ry,
    int format
)
{
    if (rx <= 0.0f || ry <= 0.0f)
        return;

    for (int r = 0; r < height; r++) {
        float ny = (y + (height - 1 - r) + 0.5f - cy) / ry;
        for (int c = 0; c < width; c++) {
            float nx = ((float)(x + c) + 0.5f - cx) / rx;
            if (nx * nx + ny * ny <= 1.0f)
                continue;

            uint8_t *p = rgba + ((size_t)r * (size_t)width + (size_t)c) * 4;
            p[0] = p[1] = p[2] = 0;
            p[3] = (format == MIRU_EXPORT_PNG || format == MIRU_EXPORT_WEBP) ? 0 : 0xFF;
        }
    }
}

int miru_export_framebuffer_region(int fb_w, int fb_h, int x, int y, int width, int height, int format, int ellipse)
{
    if (fb_w <= 0 || fb_h <= 0 || width <= 0 || height <= 0)
        return -1;

    float ecx = (float)x + (float)width * 0.5f;
    float ecy = (float)y + (float)height * 0.5f;
    float erx = (float)width * 0.5f;
    float ery = (float)height * 0.5f;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > fb_w)
        width = fb_w - x;
    if (y + height > fb_h)
        height = fb_h - y;
    if (width <= 0 || height <= 0)
        return -1;

    size_t nbytes = (size_t)width * (size_t)height * 4;

    uint8_t *rgba = malloc(nbytes);
    if (!rgba)
        return -1;

    while (glGetError() != GL_NO_ERROR) {
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, fb_h - y - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        MIRU_LOG("export: glReadPixels failed glerr=0x%x", (unsigned)err);
        free(rgba);
        return -1;
    }

    if (ellipse)
        mask_outside_ellipse(rgba, x, y, width, height, ecx, ecy, erx, ery, format);

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

    const char *ext = export_ext(format);

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

    int rc;
    switch (format) {
    case MIRU_EXPORT_BMP:
        rc = write_bmp_bgr(path, width, height, rgba);
        break;
    case MIRU_EXPORT_JPEG:
        rc = write_jpeg_rgb(path, width, height, rgba);
        break;
    case MIRU_EXPORT_WEBP:
        rc = write_webp_rgb(path, width, height, rgba);
        break;
    default:
        rc = write_png_rbga(path, width, height, rgba);
        break;
    }

    free(rgba);
    if (rc != 0) {
        MIRU_LOG("export: write failed to %s: %s", path, strerror(errno));
        return -1;
    }

    MIRU_LOG("export: wrote %s", path);
    notify_export(path);
    return 0;
}

int miru_export_framebuffer(int width, int height, int format)
{
    return miru_export_framebuffer_region(width, height, 0, 0, width, height, format, 0);
}
