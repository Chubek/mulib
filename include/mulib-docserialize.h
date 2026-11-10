/** @file mulib-docserialize.h
 * @brief Legacy JSON escaping and opt-in versioned semantic DocIR serialization.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCSERIALIZE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docserialize.md for scope and limitations.
 */
#ifndef MULIB_DOCSERIALIZE_H
#define MULIB_DOCSERIALIZE_H
#include "mulib-typecore.h"
#include "mulib-docir.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_docserialize_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_docserialize_write_cstr(mulib_typecore_buf_t *out, const char *source);
/* Stable MuDocIR binary version 1; strings, kinds, parents, styles and source ranges. */
int mulib_docserialize_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,
                          mulib_typecore_buf_t *out,mulib_docir_report_t *report);
int mulib_docserialize_read(const void *bytes,size_t length,const mulib_docir_limits_t *limits,
                          mulib_docir_node_t **root,mulib_docir_report_t *report);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCSERIALIZE_H */
#if defined(MULIB_IMPL_DOCSERIALIZE) && !defined(MULIB_DOCSERIALIZE_H_IMPLEMENTED)
#define MULIB_DOCSERIALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_docserialize_write(mulib_typecore_buf_t *out,const char *src,size_t n){if(!out||(!src&&n))return 0;return mulib_typecore_json(out,src,n);}
int mulib_docserialize_write_cstr(mulib_typecore_buf_t *out,const char *src){return src&&mulib_docserialize_write(out,src,strlen(src));}
#ifdef MULIB_ENABLE_DOCIR
#include "detail/mulib-docserialize-tree.h"
#else
int mulib_docserialize_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,mulib_typecore_buf_t *out,mulib_docir_report_t *report) {
    (void)root;(void)limits;(void)out;if(report)*report=(mulib_docir_report_t){mulib_docir_E_UNSUPPORTED,0};return 0;
}
int mulib_docserialize_read(const void *bytes,size_t length,const mulib_docir_limits_t *limits,mulib_docir_node_t **root,mulib_docir_report_t *report) {
    (void)bytes;(void)length;(void)limits;(void)root;if(report)*report=(mulib_docir_report_t){mulib_docir_E_UNSUPPORTED,0};return 0;
}
#endif
#ifdef __cplusplus
}
#endif
#endif
