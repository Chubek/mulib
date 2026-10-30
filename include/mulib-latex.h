/** @file mulib-latex.h
 * @brief Escaped LaTeX article generation with an explicit document lifecycle.
 * Define MULIB_IMPL_LATEX plus TEX, STRBUF and UTF8. No external TeX process.
 */
#ifndef MULIB_LATEX_H
#define MULIB_LATEX_H
#include "mulib-tex.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero initialize. Owns source; fields are read-only to clients. */
  typedef struct mulib_latex
  {
    mulib_strbuf_t source;
    int state;
  } mulib_latex_t;
  /** @brief Begin article with escaped title; 1 success. Inactive document
   * only. */
  int mulib_latex_begin (mulib_latex_t *document, const char *title);
  /** @brief Append escaped section heading (level 1..3); 1 success. */
  int mulib_latex_section (mulib_latex_t *document, unsigned level,
                           const char *title);
  /** @brief Append escaped paragraph plus blank line; active document only. */
  int mulib_latex_paragraph (mulib_latex_t *document, const char *text,
                             size_t length);
  /** @brief End once, idempotent; no further content may be appended. */
  int mulib_latex_finish (mulib_latex_t *document);
  /** @brief Release source/reset; accepting NULL. */
  void mulib_latex_free (mulib_latex_t *document);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LATEX) && !defined(MULIB_LATEX_IMPLEMENTED)
#define MULIB_LATEX_IMPLEMENTED
int
mulib_latex_begin (mulib_latex_t *d, const char *title)
{
  mulib_strbuf_t b = { 0 };
  int ok;
  if (!d || d->state || !title)
    return 0;
  ok = mulib_strbuf_append (&b, "\\documentclass{article}\n\\title{")
       && mulib_tex_escape (&b, title, strlen (title))
       && mulib_strbuf_append (&b, "}\n\\begin{document}\n\\maketitle\n");
  if (ok)
    {
      d->source = b;
      d->state = 1;
    }
  else
    mulib_strbuf_free (&b);
  return ok;
}
static int
mulib_latex_content_ (mulib_latex_t *d, const char *prefix, const char *s,
                      size_t n, const char *suffix)
{
  mulib_strbuf_t b = { 0 };
  int ok;
  if (!d || d->state != 1)
    return 0;
  ok = mulib_strbuf_append (&b, prefix) && mulib_tex_escape (&b, s, n)
       && mulib_strbuf_append (&b, suffix)
       && mulib_strbuf_append_n (&d->source, b.data, b.length);
  mulib_strbuf_free (&b);
  return ok;
}
int
mulib_latex_section (mulib_latex_t *d, unsigned level, const char *s)
{
  if (!s || level < 1 || level > 3)
    return 0;
  return mulib_latex_content_ (d,
                               level == 1   ? "\\section{"
                               : level == 2 ? "\\subsection{"
                                            : "\\subsubsection{",
                               s, strlen (s), "}\n");
}
int
mulib_latex_paragraph (mulib_latex_t *d, const char *s, size_t n)
{
  return mulib_latex_content_ (d, "", s, n, "\n\n");
}
int
mulib_latex_finish (mulib_latex_t *d)
{
  if (!d || !d->state)
    return 0;
  if (d->state == 2)
    return 1;
  if (!mulib_strbuf_append (&d->source, "\\end{document}\n"))
    return 0;
  d->state = 2;
  return 1;
}
void
mulib_latex_free (mulib_latex_t *d)
{
  if (d)
    {
      mulib_strbuf_free (&d->source);
      d->state = 0;
    }
}
#endif
