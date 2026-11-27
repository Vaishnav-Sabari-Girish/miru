#ifndef MIRU_EXPORT_H
#define MIRU_EXPORT_H

#define MIRU_EXPORT_BMP 0
#define MIRU_EXPORT_PNG 1
#define MIRU_EXPORT_JPEG 2
#define MIRU_EXPORT_WEBP 3

/* Read RGBA from the current GL framebuffer (bottom-left origin)
 * and write ~/Pictures/miru/miru-YYYYMMDD-HHMMSS.{bmp,png}
 * Returns 0 on success, -1 on failure
 */

int miru_export_framebuffer(int width, int height, int format);
int miru_export_framebuffer_region(int fb_w, int fb_h, int x, int y, int width, int height, int format, int ellipse);

#endif // !MIRU_EXPORT_H
