/** @file mulib-htmlwriter.h
 * @brief Self-contained semantic HTML5 document writer.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_HTMLWRITER once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-htmlwriter.md for scope and limitations.
 */
#ifndef MULIB_HTMLWRITER_H
#define MULIB_HTMLWRITER_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_htmlwriter_document(const char *title,const char *body,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTMLWRITER_H */
#if defined(MULIB_IMPL_HTMLWRITER) && !defined(MULIB_HTMLWRITER_H_IMPLEMENTED)
#define MULIB_HTMLWRITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_htmlwriter_document(const char *title,const char *body,mulib_typecore_buf_t *out){if(!title||!body||!out)return 0;mulib_typecore_buf_t b={0};int ok=mulib_typecore_text(&b,"<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>")&&mulib_typecore_html(&b,title,strlen(title))&&mulib_typecore_text(&b,"</title></head><body><p>")&&mulib_typecore_html(&b,body,strlen(body))&&mulib_typecore_text(&b,"</p></body></html>\n")&&mulib_typecore_append(out,b.data,b.size);mulib_typecore_free(&b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
