#ifndef MIRU_EXPORT_H
#define MIRU_EXPORT_H

/* Read RGBA from the current GL framebuffer (bottom-left origin)
 * and write ~/Pictures/miru/miru-YYYYMMDD-HHMMSS.bmp
 * Returns 0 on success, -1 on failure
 */

int miru_export_framebuffer(int width, int height);

#endif // !MIRU_EXPORT_H
