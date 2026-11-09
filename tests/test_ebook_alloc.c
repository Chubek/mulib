#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static size_t allocation, fail_at, live;
static void *
checked_malloc (size_t n)
{
  void *p;
  if (++allocation == fail_at)
    return NULL;
  p = malloc (n);
  if (p)
    ++live;
  return p;
}
static void *
checked_calloc (size_t n, size_t size)
{
  void *p;
  if (++allocation == fail_at)
    return NULL;
  p = calloc (n, size);
  if (p)
    ++live;
  return p;
}
static void *
checked_realloc (void *p, size_t n)
{
  void *next;
  int fresh = p == NULL;
  if (++allocation == fail_at)
    return NULL;
  next = realloc (p, n);
  if (next && fresh)
    ++live;
  return next;
}
static void
checked_free (void *p)
{
  if (p)
    {
      assert (live);
      --live;
      free (p);
    }
}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_XML
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_EPUB
#define MULIB_IMPL_MOBI
#define MULIB_IMPL_PDF
#define MULIB_IMPL_MATHML
#include "mulib-epub.h"
#include "mulib-mathml.h"
#include "mulib-mobi.h"
#include "mulib-pdf.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static const char xhtml[]
    = "<html "
      "xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>X</title></"
      "head><body><p>X</p></body></html>";
static void
run_case (unsigned which, size_t failure)
{
  mulib_buffer_t out = { 0 }, input = { 0 };
  mulib_pdf_t pdf = { 0 };
  mulib_zip_writer_t zip = { 0 };
  mulib_epub_reader_t reader = { 0 };
  size_t page = 0;
  int rc;
  mulib_epub_resource_t resource
      = { "x.xhtml", "application/xhtml+xml", "X", NULL,
          xhtml,     sizeof (xhtml) - 1 };
  mulib_epub_book_t book = {
    "urn:book", "Book", "en", NULL, "2026-11-09T00:00:00Z", &resource, 1
  };
  fail_at = 0;
  assert (mulib_buffer_append (&out, "sentinel", 8));
  if (which == 1)
    assert (mulib_epub_write (&book, &input) == 0);
  if (which == 2 || which == 3)
    {
      assert (mulib_pdf_add_page (&pdf, 100, 100, &page) == 0);
      assert (mulib_pdf_text (&pdf, page, 10, 10, 12, "initial", 7) == 0);
    }
  if (which == 4 || which == 9)
    assert (mulib_zip_add (&zip, "initial", "initial", 7, 8) == 0);
  allocation = 0;
  fail_at = failure;
  if (which == 0)
    {
      rc = mulib_epub_write (&book, &out);
      if (failure)
        assert (rc == MULIB_EPUB_ENOMEM);
      if (rc)
        assert (out.length == 8 && !memcmp (out.data, "sentinel", 8));
    }
  else if (which == 1)
    {
      rc = mulib_epub_open (&reader, input.data, input.length, 65536);
      if (failure)
        assert (rc == MULIB_EPUB_ENOMEM);
      if (rc)
        assert (!reader.zip.data && !reader.package && !reader.items
                && !reader.spine);
    }
  else if (which == 2)
    {
      rc = mulib_pdf_write (&pdf, &out);
      if (rc)
        assert (out.length == 8 && !memcmp (out.data, "sentinel", 8));
    }
  else if (which == 3)
    {
      size_t before = pdf.pages[0].content.length;
      rc = mulib_pdf_text (&pdf, page, 10, 10, 12, "additional text", 15);
      if (rc)
        assert (pdf.pages[0].content.length == before);
    }
  else if (which == 4)
    {
      size_t before = zip.bytes.length, central = zip.directory.length;
      rc = mulib_zip_add (&zip, "new", xhtml, sizeof (xhtml) - 1, 8);
      if (rc)
        assert (zip.bytes.length == before && zip.directory.length == central
                && zip.count == 1);
    }
  else if (which == 5)
    {
      mulib_xml_node_t *math = mulib_mathml_new (0);
      if (math)
        {
          mulib_xml_node_t *token = mulib_mathml_token ("mi", "x");
          if (token && !mulib_xml_append (math, token))
            mulib_xml_destroy (token);
          {
            char *s = mulib_mathml_serialize (math, NULL, NULL);
            checked_free (s);
          }
          mulib_xml_destroy (math);
        }
    }
  else if (which == 6)
    {
      static const unsigned char deflated[] = { 0x4b, 0x4c, 0x4a, 0x06, 0x00 };
      rc = mulib_deflate_decode (deflated, sizeof (deflated), 3, &out);
      if (rc)
        assert (out.length == 8 && !memcmp (out.data, "sentinel", 8));
    }
  else if (which == 7)
    {
      rc = mulib_mobi_palmdoc_decode ("abc\200\033", 5, 9, &out);
      if (rc)
        assert (out.length == 8 && !memcmp (out.data, "sentinel", 8));
    }
  else if (which == 8)
    {
      static const char source[]
          = "<math "
            "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mrow><mi>x</"
            "mi><mo>+</mo><mn>1</mn></mrow></math>";
      mulib_mathml_error_t error;
      mulib_xml_node_t *math
          = mulib_mathml_parse (source, sizeof (source) - 1, &error);
      if (failure)
        assert (!math && error.message && !error.node);
      else
        assert (math);
      mulib_xml_destroy (math);
    }
  else if (which == 9)
    {
      size_t before = zip.bytes.length;
      rc = mulib_zip_finish (&zip);
      if (failure)
        assert (rc == MULIB_ZIP_ENOMEM && !zip.finished
                && zip.bytes.length == before);
      else
        assert (rc == 0 && zip.finished);
    }
  else
    {
      page = 42;
      rc = mulib_pdf_add_page (&pdf, 100, 100, &page);
      if (failure)
        assert (rc == MULIB_PDF_ENOMEM && page == 42 && !pdf.count
                && !pdf.pages);
      else
        assert (!rc && page == 0 && pdf.count == 1);
    }
  fail_at = 0;
  mulib_epub_close (&reader);
  mulib_buffer_free (&input);
  mulib_buffer_free (&out);
  mulib_pdf_free (&pdf);
  mulib_zip_writer_free (&zip);
  assert (!live);
}
int
main (void)
{
  unsigned which;
  size_t n, total;
  for (which = 0; which < 11; ++which)
    {
      run_case (which, 0);
      total = allocation;
      for (n = 1; n <= total; ++n)
        run_case (which, n);
    }
  return 0;
}
