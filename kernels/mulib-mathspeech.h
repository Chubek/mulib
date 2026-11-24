/** @file mulib-mathspeech.h
 * @brief Checked math mathspeech text emission into an owned byte buffer.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MATHSPEECH once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mathspeech.md for scope and limitations.
 */
#ifndef MULIB_MATHSPEECH_H
#define MULIB_MATHSPEECH_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathspeech_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_mathspeech_write_cstr(mulib_typecore_buf_t *out, const char *source);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATHSPEECH_H */
#if defined(MULIB_IMPL_MATHSPEECH) && !defined(MULIB_MATHSPEECH_H_IMPLEMENTED)
#define MULIB_MATHSPEECH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathspeech_write(mulib_typecore_buf_t *out,const char *src,size_t n){if(!out||(!src&&n))return 0;return mulib_typecore_append(out,src,n);}
int mulib_mathspeech_write_cstr(mulib_typecore_buf_t *out,const char *src){return src&&mulib_mathspeech_write(out,src,strlen(src));}
#ifdef __cplusplus
}
#endif
#endif
