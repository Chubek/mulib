#ifndef ON1X_STDLIB_HTML_RUNTIME_H
#define ON1X_STDLIB_HTML_RUNTIME_H

#include <stdint.h>

/* Explicit C11 companion for html.on1x, not a general C DOM interface.
 * Input pointers are borrowed for this call. `source` has the given readable
 * length followed by NUL; other inputs are NUL-terminated. The result is an
 * On1x runtime snapshot or a static error literal. Never free it. */
const char *on1x_html_parse_v1(const char *source, int64_t length,
                               int64_t profile, int64_t hex);
const char *on1x_html_node_v1(const char *markup, int64_t fragment,
                               int64_t ordinal, int64_t operation,
                               const char *argument, int64_t index);

#endif
