#ifndef HELP_H
#define HELP_H

#include <stddef.h>
#include "keybinds.h"

const char *const *help_lines(void);
size_t help_line_count(void);
void help_set_export_format(int format);

void help_set_keybinds(const struct miru_keybinds *kb);

#endif // !HELP_H
