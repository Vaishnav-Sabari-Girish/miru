#ifndef HELP_H
#define HELP_H

#include <stddef.h>

const char *const *help_lines(void);
size_t help_line_count(void);
void help_set_export_format(int format);

#endif // !HELP_H
