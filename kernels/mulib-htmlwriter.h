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
static int mulib_htmlwriter_utf8_(const char *text, size_t len)
{
    size_t at = 0;
    uint32_t cp;
    while (at < len)
        if (!mulib_typecore_utf8((const unsigned char *)text, len, &at, &cp))
            return 0;
    return 1;
}

int mulib_htmlwriter_document(const char *title, const char *body, mulib_typecore_buf_t *out)
{
    if (!title || !body || !out) return 0;
    size_t title_len = strlen(title), body_len = strlen(body);
    if (!mulib_htmlwriter_utf8_(title, title_len) ||
        !mulib_htmlwriter_utf8_(body, body_len)) return 0;

    mulib_typecore_buf_t b = {0};
    int ok = mulib_typecore_text(&b, "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>") &&
             mulib_typecore_html(&b, title, title_len) &&
             mulib_typecore_text(&b, "</title></head><body><p>") &&
             mulib_typecore_html(&b, body, body_len) &&
             mulib_typecore_text(&b, "</p></body></html>\n") &&
             mulib_typecore_append(out, b.data, b.size);
    mulib_typecore_free(&b);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif
