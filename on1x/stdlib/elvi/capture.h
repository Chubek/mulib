#ifndef ON1X_STDLIB_ELVI_CAPTURE_H
#define ON1X_STDLIB_ELVI_CAPTURE_H

#include "mulib-elvi-script-transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit trusted companion, not a foreign import. The program and config
 * are borrowed for one bounded synchronous transaction. No inputs overlap out.
 * See the native kernel's result and ownership contract. */
mulib_elvi_script_capture_result_t on1x_elvi_capture_https(
    const mulib_elvi_scrape_program_t *program,
    const mulib_elvi_script_capture_config_t *config,
    mulib_elvi_script_document_t *out);

/* Copy an owned document's exact body as lowercase ASCII hex. Returns 1 on
 * success, 0 otherwise. Capacity includes NUL (2*body_length+1); out does not
 * overlap the document or its body and is unchanged on failure. No ownership
 * transfer; destroy the native document separately. */
int on1x_elvi_document_copy_hex(const mulib_elvi_script_document_t *document,
    char *out, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
