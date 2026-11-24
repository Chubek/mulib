/** @file mulib-docnormalize.h
 * @brief Legacy byte emission and opt-in owned DocIR text-node normalization.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCNORMALIZE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docnormalize.md for scope and limitations.
 */
#ifndef MULIB_DOCNORMALIZE_H
#define MULIB_DOCNORMALIZE_H
#include "mulib-typecore.h"
#include "mulib-docir.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_docnormalize_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_docnormalize_write_cstr(mulib_typecore_buf_t *out, const char *source);
mulib_docir_node_t *mulib_docnormalize_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,mulib_docir_report_t *report);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCNORMALIZE_H */
#if defined(MULIB_IMPL_DOCNORMALIZE) && !defined(MULIB_DOCNORMALIZE_H_IMPLEMENTED)
#define MULIB_DOCNORMALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_docnormalize_write(mulib_typecore_buf_t *out,const char *src,size_t n){if(!out||(!src&&n))return 0;return mulib_typecore_append(out,src,n);}
int mulib_docnormalize_write_cstr(mulib_typecore_buf_t *out,const char *src){return src&&mulib_docnormalize_write(out,src,strlen(src));}
mulib_docir_node_t *mulib_docnormalize_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,mulib_docir_report_t *report) {
#ifdef MULIB_ENABLE_DOCIR
    return mulib_docir_clone(root,limits,mulib_docir_NORMALIZE_TEXT,report);
#else
    (void)root;(void)limits;if(report)*report=(mulib_docir_report_t){mulib_docir_E_UNSUPPORTED,0};return NULL;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
