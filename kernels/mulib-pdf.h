/** @file mulib-pdf.h
 * @brief Deterministic PDF 1.4 generation: pages, Helvetica text and vector
 * paths. Define MULIB_IMPL_PDF, MULIB_IMPL_BUFFER and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_PDF_H
#define MULIB_PDF_H
#include "mulib-buffer.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Result codes. All mutating operations are failure-atomic. */
  enum mulib_pdf_result
  {
    MULIB_PDF_OK = 0,
    MULIB_PDF_EINVAL = -1,
    MULIB_PDF_ENOMEM = -2,
    MULIB_PDF_ELIMIT = -3
  };
  typedef struct mulib_pdf_page mulib_pdf_page_t;
  /** @brief Zero initialize; owns pages. Fields are read-only; do not copy. */
  typedef struct mulib_pdf
  {
    mulib_pdf_page_t *pages;
    size_t count;
  } mulib_pdf_t;
  /** @brief Add a page of width/height in points (0.001 <= value <= 14400).
   * On success index receives a stable zero-based page index. */
  int mulib_pdf_add_page (mulib_pdf_t *pdf, double width, double height,
                          size_t *index);
  /** @brief Set both fill/stroke RGB (components 0..1) for subsequent
   * operations. */
  int mulib_pdf_color (mulib_pdf_t *pdf, size_t page, double red, double green,
                       double blue);
  /** @brief Draw printable ASCII text at a baseline in bottom-left-origin
   * points. Helvetica, size 0.001 <= size <= 1000. Escapes PDF syntax; no
   * wrapping/shaping. Empty (NULL,0) is accepted. Coordinates have 0.001-point
   * precision. */
  int mulib_pdf_text (mulib_pdf_t *pdf, size_t page, double x, double y,
                      double size, const char *text, size_t length);
  /** @brief Stroke a segment using width 0..1000 points (zero is a hairline).
   */
  int mulib_pdf_line (mulib_pdf_t *pdf, size_t page, double x1, double y1,
                      double x2, double y2, double width);
  /** @brief Draw rectangle; fill=1 fills, fill=0 strokes with given line
   * width. Rectangle dimensions are nonnegative, coordinates within
   * +/-1000000. */
  int mulib_pdf_rect (mulib_pdf_t *pdf, size_t page, double x, double y,
                      double width, double height, int fill,
                      double line_width);
  /** @brief Append a complete PDF with classic xref/trailer. At least one page
   * is required. Does not freeze pdf; input document remains reusable. */
  int mulib_pdf_write (const mulib_pdf_t *pdf, mulib_buffer_t *out);
  /** @brief Release all pages and reset. NULL is harmless. */
  void mulib_pdf_free (mulib_pdf_t *pdf);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PDF) && !defined(MULIB_PDF_IMPLEMENTED)
#define MULIB_PDF_IMPLEMENTED
#include <stdarg.h>
struct mulib_pdf_page
{
  mulib_strbuf_t content;
  double width, height;
};
static int
mulib_pdf_printf_ (mulib_strbuf_t *out, const char *format, ...)
{
  char local[256], *p = local;
  va_list args, copy;
  int n, ok;
  va_start (args, format);
  va_copy (copy, args);
  n = vsnprintf (local, sizeof (local), format, args);
  va_end (args);
  if (n < 0)
    {
      va_end (copy);
      return 0;
    }
  if ((size_t)n >= sizeof (local))
    {
      p = (char *)malloc ((size_t)n + 1);
      if (!p)
        {
          va_end (copy);
          return 0;
        }
      (void)vsnprintf (p, (size_t)n + 1, format, copy);
    }
  va_end (copy);
  ok = mulib_strbuf_append_n (out, p, (size_t)n);
  if (p != local)
    free (p);
  return ok;
}
static int
mulib_pdf_number_ (mulib_strbuf_t *out, double v)
{
  uint64_t n;
  if (!isfinite (v) || v < -1000000 || v > 1000000)
    return 0;
  n = (uint64_t)((v < 0 ? -v : v) * 1000 + 0.5);
  return mulib_pdf_printf_ (out, "%s%llu.%03u", v < 0 && n ? "-" : "",
                            (unsigned long long)(n / 1000),
                            (unsigned)(n % 1000));
}
static int
mulib_pdf_coord_ (double v)
{
  return isfinite (v) && v >= -1000000 && v <= 1000000;
}
static int
mulib_pdf_page_valid_ (const mulib_pdf_t *d, size_t page)
{
  return d && page < d->count;
}
static int
mulib_pdf_commit_ (mulib_pdf_t *d, size_t page, mulib_strbuf_t *b, int ok)
{
  if (ok)
    ok = mulib_strbuf_append_n (&d->pages[page].content, b->data, b->length);
  mulib_strbuf_free (b);
  return ok ? 0 : MULIB_PDF_ENOMEM;
}
int
mulib_pdf_add_page (mulib_pdf_t *d, double width, double height, size_t *index)
{
  mulib_pdf_page_t *p;
  if (!d || !index || !isfinite (width) || !isfinite (height) || width < 0.001
      || height < 0.001 || width > 14400 || height > 14400)
    return MULIB_PDF_EINVAL;
  if (d->count >= 100000 || d->count >= SIZE_MAX / sizeof (*p) - 1)
    return MULIB_PDF_ELIMIT;
  p = (mulib_pdf_page_t *)realloc (d->pages, (d->count + 1) * sizeof (*p));
  if (!p)
    return MULIB_PDF_ENOMEM;
  d->pages = p;
  memset (&p[d->count], 0, sizeof (*p));
  p[d->count].width = width;
  p[d->count].height = height;
  *index = d->count++;
  return 0;
}
int
mulib_pdf_color (mulib_pdf_t *d, size_t page, double r, double g, double b)
{
  mulib_strbuf_t s = { 0 };
  int ok;
  if (!mulib_pdf_page_valid_ (d, page) || !isfinite (r) || !isfinite (g)
      || !isfinite (b) || r < 0 || r > 1 || g < 0 || g > 1 || b < 0 || b > 1)
    return MULIB_PDF_EINVAL;
  ok = mulib_pdf_number_ (&s, r) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, g) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, b) && mulib_strbuf_append (&s, " rg\n")
       && mulib_pdf_number_ (&s, r) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, g) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, b) && mulib_strbuf_append (&s, " RG\n");
  return mulib_pdf_commit_ (d, page, &s, ok);
}
int
mulib_pdf_text (mulib_pdf_t *d, size_t page, double x, double y, double size,
                const char *text, size_t n)
{
  mulib_strbuf_t s = { 0 };
  size_t i;
  int ok;
  if (!mulib_pdf_page_valid_ (d, page) || !mulib_pdf_coord_ (x)
      || !mulib_pdf_coord_ (y) || !isfinite (size) || size < 0.001
      || size > 1000 || (!text && n))
    return MULIB_PDF_EINVAL;
  for (i = 0; i < n; ++i)
    if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126)
      return MULIB_PDF_EINVAL;
  ok = mulib_strbuf_append (&s, "BT /F1 ") && mulib_pdf_number_ (&s, size)
       && mulib_strbuf_append (&s, " Tf 1 0 0 1 ") && mulib_pdf_number_ (&s, x)
       && mulib_strbuf_putc (&s, ' ') && mulib_pdf_number_ (&s, y)
       && mulib_strbuf_append (&s, " Tm (");
  for (i = 0; ok && i < n; ++i)
    {
      if (text[i] == '(' || text[i] == ')' || text[i] == '\\')
        ok = mulib_strbuf_putc (&s, '\\');
      if (ok)
        ok = mulib_strbuf_putc (&s, text[i]);
    }
  if (ok)
    ok = mulib_strbuf_append (&s, ") Tj ET\n");
  return mulib_pdf_commit_ (d, page, &s, ok);
}
int
mulib_pdf_line (mulib_pdf_t *d, size_t page, double x1, double y1, double x2,
                double y2, double width)
{
  mulib_strbuf_t s = { 0 };
  int ok;
  if (!mulib_pdf_page_valid_ (d, page) || !mulib_pdf_coord_ (x1)
      || !mulib_pdf_coord_ (y1) || !mulib_pdf_coord_ (x2)
      || !mulib_pdf_coord_ (y2) || !isfinite (width) || width < 0
      || width > 1000)
    return MULIB_PDF_EINVAL;
  ok = mulib_pdf_number_ (&s, width) && mulib_strbuf_append (&s, " w ")
       && mulib_pdf_number_ (&s, x1) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, y1) && mulib_strbuf_append (&s, " m ")
       && mulib_pdf_number_ (&s, x2) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, y2) && mulib_strbuf_append (&s, " l S\n");
  return mulib_pdf_commit_ (d, page, &s, ok);
}
int
mulib_pdf_rect (mulib_pdf_t *d, size_t page, double x, double y, double width,
                double height, int fill, double line_width)
{
  mulib_strbuf_t s = { 0 };
  int ok;
  if (!mulib_pdf_page_valid_ (d, page) || !mulib_pdf_coord_ (x)
      || !mulib_pdf_coord_ (y) || !mulib_pdf_coord_ (width)
      || !mulib_pdf_coord_ (height) || width < 0 || height < 0
      || !isfinite (line_width) || line_width < 0 || line_width > 1000
      || (fill != 0 && fill != 1))
    return MULIB_PDF_EINVAL;
  ok = mulib_pdf_number_ (&s, line_width) && mulib_strbuf_append (&s, " w ")
       && mulib_pdf_number_ (&s, x) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, y) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, width) && mulib_strbuf_putc (&s, ' ')
       && mulib_pdf_number_ (&s, height)
       && mulib_strbuf_append (&s, fill ? " re f\n" : " re S\n");
  return mulib_pdf_commit_ (d, page, &s, ok);
}
int
mulib_pdf_write (const mulib_pdf_t *d, mulib_buffer_t *out)
{
  mulib_strbuf_t s = { 0 };
  size_t *offsets, objects, i, xref, total = 0;
  int result = MULIB_PDF_ENOMEM;
  if (!d || !out || !d->count)
    return MULIB_PDF_EINVAL;
  if (d->count > 100000)
    return MULIB_PDF_ELIMIT;
  /* Check content totals before copying streams; a huge document must fail
   * without first attempting an equally huge temporary allocation. */
  for (i = 0; i < d->count; ++i)
    {
      if (d->pages[i].content.length > UINT32_MAX - total)
        return MULIB_PDF_ELIMIT;
      total += d->pages[i].content.length;
    }
  objects = 3 + d->count * 2;
  offsets = (size_t *)calloc (objects + 1, sizeof (*offsets));
  if (!offsets)
    return result;
  if (!mulib_strbuf_append (&s, "%PDF-1.4\n%\342\343\317\323\n"))
    goto done;
  offsets[1] = s.length;
  if (!mulib_strbuf_append (
          &s, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"))
    goto done;
  offsets[2] = s.length;
  if (!mulib_pdf_printf_ (&s, "2 0 obj\n<< /Type /Pages /Count %zu /Kids [",
                          d->count))
    goto done;
  for (i = 0; i < d->count; ++i)
    if (!mulib_pdf_printf_ (&s, "%zu 0 R ", 4 + i * 2))
      goto done;
  if (!mulib_strbuf_append (&s, "] >>\nendobj\n"))
    goto done;
  offsets[3] = s.length;
  if (!mulib_strbuf_append (
          &s, "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
              "/Encoding /WinAnsiEncoding >>\nendobj\n"))
    goto done;
  for (i = 0; i < d->count; ++i)
    {
      const mulib_pdf_page_t *p = &d->pages[i];
      size_t id = 4 + i * 2;
      offsets[id] = s.length;
      if (!mulib_pdf_printf_ (
              &s, "%zu 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ",
              id)
          || !mulib_pdf_number_ (&s, p->width) || !mulib_strbuf_putc (&s, ' ')
          || !mulib_pdf_number_ (&s, p->height)
          || !mulib_pdf_printf_ (&s,
                                 "] /Resources << /Font << /F1 3 0 R >> >> "
                                 "/Contents %zu 0 R >>\nendobj\n",
                                 id + 1))
        goto done;
      offsets[id + 1] = s.length;
      if (s.length > UINT32_MAX || p->content.length > UINT32_MAX - s.length
          || UINT32_MAX - s.length - p->content.length < 96)
        {
          result = MULIB_PDF_ELIMIT;
          goto done;
        }
      if (!mulib_pdf_printf_ (&s, "%zu 0 obj\n<< /Length %zu >>\nstream\n",
                              id + 1, p->content.length)
          || !mulib_strbuf_append_n (&s, p->content.data, p->content.length)
          || !mulib_strbuf_append (&s, "\nendstream\nendobj\n"))
        goto done;
      if (s.length > UINT32_MAX)
        {
          result = MULIB_PDF_ELIMIT;
          goto done;
        }
    }
  xref = s.length;
  if (!mulib_pdf_printf_ (&s, "xref\n0 %zu\n0000000000 65535 f \n",
                          objects + 1))
    goto done;
  for (i = 1; i <= objects; ++i)
    if (!mulib_pdf_printf_ (&s, "%010zu 00000 n \n", offsets[i]))
      goto done;
  if (!mulib_pdf_printf_ (
          &s,
          "trailer\n<< /Size %zu /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
          objects + 1, xref))
    goto done;
  result = mulib_buffer_append (out, s.data, s.length) ? 0 : MULIB_PDF_ENOMEM;
done:
  free (offsets);
  mulib_strbuf_free (&s);
  return result;
}
void
mulib_pdf_free (mulib_pdf_t *d)
{
  if (d)
    {
      size_t i;
      for (i = 0; i < d->count; ++i)
        mulib_strbuf_free (&d->pages[i].content);
      free (d->pages);
      memset (d, 0, sizeof (*d));
    }
}
#endif
