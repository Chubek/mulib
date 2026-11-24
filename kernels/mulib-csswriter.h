/** @file mulib-csswriter.h
 * @brief Checked output csswriter text emission into an owned byte buffer.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_CSSWRITER once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-csswriter.md for scope and limitations.
 */
#ifndef MULIB_CSSWRITER_H
#define MULIB_CSSWRITER_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_csswriter_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_csswriter_write_cstr(mulib_typecore_buf_t *out, const char *source);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSSWRITER_H */
#if defined(MULIB_IMPL_CSSWRITER) && !defined(MULIB_CSSWRITER_H_IMPLEMENTED)
#define MULIB_CSSWRITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_csswriter_write(mulib_typecore_buf_t *out,const char *src,size_t n){if(!out||(!src&&n))return 0;return mulib_typecore_append(out,src,n);}
int mulib_csswriter_write_cstr(mulib_typecore_buf_t *out,const char *src){return src&&mulib_csswriter_write(out,src,strlen(src));}
#ifdef __cplusplus
}
#endif
#endif
