/** @file mulib-latexwriter.h
 * @brief Escaped LaTeX article source generator.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_LATEXWRITER once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-latexwriter.md for scope and limitations.
 */
#ifndef MULIB_LATEXWRITER_H
#define MULIB_LATEXWRITER_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_latexwriter_document(const char *title,const char *body,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LATEXWRITER_H */
#if defined(MULIB_IMPL_LATEXWRITER) && !defined(MULIB_LATEXWRITER_H_IMPLEMENTED)
#define MULIB_LATEXWRITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_latexwriter_document(const char *title,const char *body,mulib_typecore_buf_t *out){if(!title||!body||!out)return 0;mulib_typecore_buf_t b={0};int ok=mulib_typecore_text(&b,"\\documentclass{article}\n\\usepackage[T1]{fontenc}\n\\title{")&&mulib_typecore_latex(&b,title,strlen(title))&&mulib_typecore_text(&b,"}\n\\begin{document}\n\\maketitle\n")&&mulib_typecore_latex(&b,body,strlen(body))&&mulib_typecore_text(&b,"\n\\end{document}\n")&&mulib_typecore_append(out,b.data,b.size);mulib_typecore_free(&b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
