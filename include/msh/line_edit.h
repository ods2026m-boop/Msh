#ifndef MSH_LINE_EDIT_H
#define MSH_LINE_EDIT_H

#include "msh.h"

/* Read a line with raw-mode terminal editing.
 * Supports: left/right arrow, backspace, delete, home/end,
 *           Ctrl-D (EOF), Ctrl-C (interrupt), Up/Down for history,
 *           Tab for completion.
 * Returns the length of buf on success, -1 on EOF.
 */
int msh_line_edit_read(char *buf, size_t max, const char *prompt);

#endif