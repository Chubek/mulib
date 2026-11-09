#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_XML
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_EPUB
#define MULIB_IMPL_MOBI
#define MULIB_IMPL_AZW3
#define MULIB_IMPL_PDF
#define MULIB_IMPL_MATHML
#include "mulib-azw3.h"
#include "mulib-epub.h"
#include "mulib-mathml.h"
#include "mulib-pdf.h"
#include <assert.h>

static const char chapter[]
    = "<html "
      "xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Chapter</title></"
      "head><body><p>Hello EPUB &amp; XML.</p></body></html>";
static const char css[] = "body { color: navy; }";
static void
be32 (unsigned char *p, uint32_t v)
{
  size_t i;
  for (i = 0; i < 4; ++i)
    {
      p[3 - i] = (unsigned char)v;
      v >>= 8;
    }
}
static void
be16 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)(v >> 8);
  p[1] = (unsigned char)v;
}
static void
le32 (unsigned char *p, uint32_t v)
{
  size_t i;
  for (i = 0; i < 4; ++i)
    {
      p[i] = (unsigned char)v;
      v >>= 8;
    }
}

static void
test_deflate (void)
{
  unsigned char input[70000];
  size_t sizes[] = { 0, 1, 2, 3, 258, 4096, sizeof (input) }, i, j;
  uint32_t state = 17;
  const unsigned char stored[] = { 1, 3, 0, 252, 255, 'a', 'b', 'c' };
  const unsigned char bad[] = { 7 };
  mulib_buffer_t out = { 0 };
  for (i = 0; i < sizeof (input); ++i)
    {
      state = state * 1664525u + 1013904223u;
      input[i] = (unsigned char)(state >> 24);
    }
  for (j = 0; j < 2; ++j)
    {
      if (j)
        for (i = 0; i < sizeof (input); ++i)
          input[i] = (unsigned char)(i % 7);
      for (i = 0; i < sizeof (sizes) / sizeof (sizes[0]); ++i)
        {
          mulib_buffer_t encoded = { 0 }, decoded = { 0 };
          size_t n = sizes[i];
          assert (mulib_deflate_encode (input, n, &encoded) == 0);
          assert (
              mulib_deflate_decode (encoded.data, encoded.length, n, &decoded)
              == 0);
          assert (decoded.length == n
                  && (!n || !memcmp (decoded.data, input, n)));
          if (n)
            {
              assert (mulib_deflate_decode (encoded.data, encoded.length,
                                            n - 1, &decoded)
                      == MULIB_DEFLATE_ELIMIT);
              assert (decoded.length == n);
            }
          assert (mulib_deflate_decode (encoded.data, encoded.length - 1, n,
                                        &decoded)
                  == MULIB_DEFLATE_EFORMAT);
          assert (mulib_buffer_append (&encoded, "x", 1));
          assert (
              mulib_deflate_decode (encoded.data, encoded.length, n, &decoded)
              == MULIB_DEFLATE_EFORMAT);
          mulib_buffer_free (&encoded);
          mulib_buffer_free (&decoded);
        }
    }
  assert (mulib_deflate_decode (stored, sizeof (stored), 3, &out) == 0
          && out.length == 3);
  assert (!memcmp (out.data, "abc", 3));
  assert (mulib_deflate_decode (bad, sizeof (bad), 100, &out)
              == MULIB_DEFLATE_EFORMAT
          && out.length == 3);
  assert (mulib_deflate_decode (NULL, 0, 100, &out) == MULIB_DEFLATE_EFORMAT);
  assert (mulib_deflate_encode (NULL, 1, &out) == MULIB_DEFLATE_EINVAL);
  assert (mulib_deflate_encode (out.data, out.length, &out) == 0);
  mulib_buffer_free (&out);
}
static void
test_zip (void)
{
  mulib_zip_writer_t w = { 0 };
  mulib_zip_reader_t r = { 0 };
  mulib_buffer_t out = { 0 };
  size_t n, i;
  assert (!mulib_zip_safe_path ("../bad") && !mulib_zip_safe_path ("a//b")
          && !mulib_zip_safe_path ("/a"));
  assert (!mulib_zip_safe_path ("a/../b") && !mulib_zip_safe_path ("C:bad")
          && mulib_zip_safe_path ("a/b/"));
  assert (mulib_zip_add (&w, "empty", NULL, 0, 0) == 0);
  assert (mulib_zip_add (&w, "stored", "hello", 5, 0) == 0);
  assert (mulib_zip_add (&w, "deflated", chapter, sizeof (chapter) - 1, 8)
          == 0);
  n = w.bytes.length;
  assert (mulib_zip_add (&w, "stored", "duplicate", 9, 0) == MULIB_ZIP_EINVAL
          && w.bytes.length == n);
  assert (mulib_zip_add (&w, "a", NULL, 0, 99) == MULIB_ZIP_EUNSUPPORTED);
  assert (mulib_zip_finish (&w) == 0);
  n = w.bytes.length;
  assert (mulib_zip_finish (&w) == 0 && w.bytes.length == n);
  assert (mulib_zip_add (&w, "after", NULL, 0, 0) == MULIB_ZIP_EINVAL);
  assert (mulib_zip_open (&r, w.bytes.data, n) == 0 && r.count == 3);
  assert (mulib_zip_extract (&r, 0, 0, &out) == 0 && !out.length);
  assert (mulib_zip_extract (&r, 2, 1, &out) == MULIB_ZIP_ELIMIT
          && !out.length);
  assert (mulib_zip_extract (&r, 2, 65536, &out) == 0
          && out.length == sizeof (chapter) - 1);
  assert (!memcmp (out.data, chapter, out.length));
  w.bytes.data[r.entries[1].data_offset] ^= 1;
  assert (mulib_zip_extract (&r, 1, 5, &out) == MULIB_ZIP_ECRC);
  w.bytes.data[r.entries[1].data_offset] ^= 1;
  mulib_zip_close (&r);
  for (i = 0; i < n; ++i)
    {
      assert (mulib_zip_open (&r, w.bytes.data, i) != 0);
      assert (!r.data && !r.entries);
    }
  /* Reject multi-disk and malformed local offsets, preserving reader. */
  w.bytes.data[n - 18] = 1;
  assert (mulib_zip_open (&r, w.bytes.data, n) == MULIB_ZIP_EUNSUPPORTED);
  w.bytes.data[n - 18] = 0;
  {
    /* A false EOCD signature embedded in a comment must not hide the real
     * directory. Its comment length matches, but its directory offset does
     * not. */
    unsigned char comment[22] = { 0 };
    w.bytes.data[n - 2] = 22;
    le32 (comment, UINT32_C (0x06054b50));
    assert (mulib_buffer_append (&w.bytes, comment, sizeof (comment)));
    assert (mulib_zip_open (&r, w.bytes.data, w.bytes.length) == 0
            && r.count == 3);
    mulib_zip_close (&r);
    le32 (w.bytes.data + n + 16, UINT32_MAX);
    assert (mulib_zip_open (&r, w.bytes.data, w.bytes.length) == 0
            && r.count == 3);
    mulib_zip_close (&r);
    le32 (w.bytes.data + n + 12, 1);
    le32 (w.bytes.data + n + 16, (uint32_t)(n - 1));
    assert (mulib_zip_open (&r, w.bytes.data, w.bytes.length) == 0
            && r.count == 3);
    mulib_zip_close (&r);
    w.bytes.length = n;
    w.bytes.data[n - 2] = 0;
  }
  w.bytes.data[4] = 10; /* local/central required versions must agree */
  assert (mulib_zip_open (&r, w.bytes.data, n) == MULIB_ZIP_EFORMAT);
  w.bytes.data[4] = 20;
  w.bytes.data[10] ^= 1; /* local/central timestamps must agree */
  assert (mulib_zip_open (&r, w.bytes.data, n) == MULIB_ZIP_EFORMAT);
  w.bytes.data[10] ^= 1;
  mulib_buffer_free (&out);
  mulib_zip_writer_free (&w);
  assert (mulib_zip_finish (&w) == 0
          && mulib_zip_open (&r, w.bytes.data, w.bytes.length) == 0
          && !r.count);
  mulib_zip_close (&r);
  mulib_zip_writer_free (&w);
}
static void
make_epub (mulib_buffer_t *bytes)
{
  mulib_epub_resource_t resources[]
      = { { "chapter.xhtml", "application/xhtml+xml", "Chapter & One", NULL,
            chapter, sizeof (chapter) - 1 },
          { "style.css", "text/css", NULL, NULL, css, sizeof (css) - 1 } };
  mulib_epub_book_t book
      = { "urn:test:book",        "A <Book>", "en", "Author & Co",
          "2026-11-09T12:30:00Z", resources,  2 };
  assert (mulib_epub_write (&book, bytes) == 0);
  book.modified = "2026-02-29T12:00:00Z";
  {
    size_t n = bytes->length;
    assert (mulib_epub_write (&book, bytes) == MULIB_EPUB_EINVAL
            && bytes->length == n);
  }
}
static void
test_epub (void)
{
  mulib_buffer_t bytes = { 0 }, out = { 0 };
  mulib_epub_reader_t r = { 0 };
  size_t i;
  make_epub (&bytes);
  assert (mulib_epub_open (&r, NULL, 1, 65536) == MULIB_EPUB_EINVAL);
  assert (mulib_epub_open (&r, bytes.data, bytes.length, 1)
              == MULIB_EPUB_ELIMIT
          && !r.zip.data);
  assert (mulib_epub_open (&r, bytes.data, bytes.length, 65536) == 0);
  assert (r.item_count == 3 && r.spine_count == 1
          && !strcmp (r.package_path, "EPUB/package.opf"));
  assert (mulib_epub_find (&r, "missing") == SIZE_MAX);
  assert (mulib_epub_read (&r, r.spine[0], 65536, &out) == 0);
  assert (out.length == sizeof (chapter) - 1
          && !memcmp (out.data, chapter, out.length));
  assert (mulib_epub_read (&r, r.spine[0], 1, &out) == MULIB_EPUB_ELIMIT
          && out.length == sizeof (chapter) - 1);
  mulib_epub_close (&r);
  mulib_buffer_free (&out);
  for (i = 0; i < bytes.length; i += 17)
    assert (mulib_epub_open (&r, bytes.data, i, 65536) != 0 && !r.zip.data);
  bytes.data[38] = 'X';
  assert (mulib_epub_open (&r, bytes.data, bytes.length, 65536)
          == MULIB_EPUB_EFORMAT);
  mulib_buffer_free (&bytes);
}

static void
mobi_header (unsigned char *h, unsigned version, size_t text_length,
             int hybrid)
{
  memset (h, 0, 320);
  be16 (h, 1);
  be32 (h + 4, (uint32_t)text_length);
  be16 (h + 8, 1);
  be16 (h + 10, 4096);
  memcpy (h + 16, "MOBI", 4);
  be32 (h + 20, 248);
  be32 (h + 24, 2);
  be32 (h + 28, 65001);
  be32 (h + 36, version);
  be32 (h + 84, 316);
  be32 (h + 88, 4);
  memcpy (h + 316, "Book", 4);
  be32 (h + 104, version);
  be32 (h + 108, UINT32_MAX);
  be32 (h + 192, 2);
  be32 (h + 196, 2);
  be32 (h + 128, 64);
  memcpy (h + 264, "EXTH", 4);
  be32 (h + 268, hybrid ? 48 : 36);
  be32 (h + 272, hybrid ? 3 : 2);
  be32 (h + 276, 100);
  be32 (h + 280, 12);
  memcpy (h + 284, "John", 4);
  be32 (h + 288, 100);
  be32 (h + 292, 12);
  memcpy (h + 296, "Jane", 4);
  if (hybrid)
    {
      be32 (h + 300, 121);
      be32 (h + 304, 12);
      be32 (h + 308, 2);
    }
}
static size_t
make_mobi (unsigned char *p, unsigned version, int hybrid)
{
  const char text[] = "<p>x</p>body{color:red}";
  unsigned char h[320], h8[320], fdst[28] = { 0 };
  size_t pos, count = hybrid ? 6 : 3, i;
  mulib_mobi_span_t records[6];
  mobi_header (h, version, sizeof (text) - 1, hybrid);
  mobi_header (h8, 8, sizeof (text) - 1, 0);
  memcpy (fdst, "FDST", 4);
  be32 (fdst + 4, 12);
  be32 (fdst + 8, 2);
  be32 (fdst + 12, 0);
  be32 (fdst + 16, 8);
  be32 (fdst + 20, 8);
  be32 (fdst + 24, sizeof (text) - 1);
  records[0].data = h;
  records[0].size = sizeof (h);
  records[1].data = (const unsigned char *)text;
  records[1].size = sizeof (text) - 1;
  if (hybrid)
    {
      records[2].data = (const unsigned char *)"BOUNDARY";
      records[2].size = 8;
      records[3].data = h8;
      records[3].size = sizeof (h8);
      records[4] = records[1];
    }
  records[count - 1].data = fdst;
  records[count - 1].size = sizeof (fdst);
  memset (p, 0, 2048);
  memcpy (p + 60, "BOOKMOBI", 8);
  be16 (p + 76, (unsigned)count);
  pos = 78 + count * 8 + 2;
  for (i = 0; i < count; ++i)
    {
      be32 (p + 78 + i * 8, (uint32_t)pos);
      memcpy (p + pos, records[i].data, records[i].size);
      pos += records[i].size;
    }
  return pos;
}
static void
test_mobi (void)
{
  unsigned char pdb[2048],
      compressed[] = { 'a', 'b', 'c', 0x80, 0x1b, 0xc1, 3, 0, 1, 2 };
  mulib_mobi_reader_t r;
  mulib_mobi_span_t s;
  mulib_azw3_reader_t az;
  mulib_buffer_t text = { 0 };
  size_t n, i, start, offset, length;
  assert (
      mulib_mobi_palmdoc_decode (compressed, sizeof (compressed), 100, &text)
      == 0);
  assert (text.length == 14 && !memcmp (text.data, "abcabcabc A\0\1\2", 14));
  mulib_buffer_free (&text);
  assert (mulib_mobi_palmdoc_decode ("\200\000", 2, 100, &text)
          == MULIB_MOBI_EFORMAT);
  assert (mulib_mobi_palmdoc_decode ("\003x", 2, 100, &text)
          == MULIB_MOBI_EFORMAT);
  assert (mulib_mobi_palmdoc_decode (compressed, sizeof (compressed), 2, &text)
              == MULIB_MOBI_ELIMIT
          && !text.length);
  n = make_mobi (pdb, 6, 0);
  assert (mulib_mobi_open (&r, pdb, n) == 0 && r.version == 6
          && r.encoding == 65001);
  assert (r.record_size == 4096);
  assert (r.title.size == 4 && !memcmp (r.title.data, "Book", 4));
  assert (mulib_mobi_metadata (&r, 100, 1, &s) == 0 && s.size == 4
          && !memcmp (s.data, "Jane", 4));
  assert (mulib_mobi_metadata (&r, 100, 2, &s) == MULIB_MOBI_NOTFOUND);
  assert (mulib_mobi_text (&r, 100, &text) == 0 && text.length == 23);
  assert (mulib_mobi_text (&r, 1, &text) == MULIB_MOBI_ELIMIT
          && text.length == 23);
  assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EUNSUPPORTED);
  mulib_buffer_free (&text);
  start = 78 + 3 * 8 + 2;
  {
    mulib_mobi_reader_t saved = r;
    be16 (pdb + start + 10, 0);
    assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EFORMAT
            && !memcmp (&r, &saved, sizeof (r)));
    be16 (pdb + start + 10, 22);
    assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EFORMAT);
    be16 (pdb + start + 10, 4096);
    be32 (pdb + start + 108, 1);
    assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EFORMAT);
    be32 (pdb + start + 108, UINT32_MAX);
  }
  be16 (pdb + start + 12, 1);
  assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EENCRYPTED);
  be16 (pdb + start + 12, 0);
  be16 (pdb + start, 0x4448);
  assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EUNSUPPORTED);
  be16 (pdb + start, 1);
  be32 (pdb + start + 280, 0);
  assert (mulib_mobi_open (&r, pdb, n) == MULIB_MOBI_EFORMAT);
  n = make_mobi (pdb, 8, 0);
  assert (mulib_azw3_open (&az, pdb, n) == 0 && az.flow_count == 2);
  {
    mulib_azw3_reader_t saved = az;
    be32 (pdb + start + 192, 1);
    assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EFORMAT
            && !memcmp (&az, &saved, sizeof (az)));
    be32 (pdb + start + 192, 2);
    pdb[n] = 0;
    assert (mulib_azw3_open (&az, pdb, n + 1) == MULIB_MOBI_EFORMAT);
  }
  assert (mulib_azw3_flow_range (&az, 1, &offset, &length) == 0 && offset == 8
          && length == 15);
  assert (mulib_azw3_flow (&az, 1, 100, &text) == 0 && text.length == 15
          && !memcmp (text.data, "body{color:red}", 15));
  mulib_buffer_free (&text);
  for (i = 0; i < n; ++i)
    assert (mulib_azw3_open (&az, pdb, i) != 0);
  be32 (pdb + n - 4, 1000);
  assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EFORMAT);
  n = make_mobi (pdb, 6, 1);
  assert (mulib_azw3_open (&az, pdb, n) == 0 && az.mobi.header_record == 3
          && az.mobi.version == 8);
  assert (mulib_azw3_flow (&az, 0, 100, &text) == 0 && text.length == 8
          && !memcmp (text.data, "<p>x</p>", 8));
  mulib_buffer_free (&text);
  start = 78 + 6 * 8 + 2;
  be32 (pdb + start + 308, UINT32_MAX);
  assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EUNSUPPORTED);
  be32 (pdb + start + 308, 1);
  assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EFORMAT);
  be32 (pdb + start + 308, 2);
  be32 (pdb + start + 276, 121); /* ambiguous duplicate KF8 boundary */
  be32 (pdb + start + 284, 2);
  assert (mulib_azw3_open (&az, pdb, n) == MULIB_MOBI_EFORMAT);
  /* Public reader path: strip a trailer and UTF-8 overlap before PalmDOC
   * decompression. A malformed trailer must preserve the existing output. */
  {
    unsigned char raw[] = { 'h', 'i', 0, 'x', 0x82 };
    memset (pdb, 0, sizeof (pdb));
    memcpy (pdb + 60, "BOOKMOBI", 8);
    be16 (pdb + 76, 2);
    be32 (pdb + 78, 96);
    be32 (pdb + 86, 416);
    mobi_header (pdb + 96, 6, 2, 0);
    be16 (pdb + 96, 2);
    be16 (pdb + 96 + 242, 3);
    memcpy (pdb + 416, raw, sizeof (raw));
    assert (mulib_mobi_open (&r, pdb, 421) == 0);
    assert (mulib_mobi_text (&r, 2, &text) == 0 && text.length == 2
            && !memcmp (text.data, "hi", 2));
    pdb[420] = 0x80;
    assert (mulib_mobi_open (&r, pdb, 421) == 0);
    assert (mulib_mobi_text (&r, 2, &text) == MULIB_MOBI_EFORMAT
            && text.length == 2);
    mulib_buffer_free (&text);
  }
  {
    /* Two compressed records whose total matches the declared text length,
     * but the first expands beyond the declared per-record bound. */
    memset (pdb, 0, sizeof (pdb));
    memcpy (pdb + 60, "BOOKMOBI", 8);
    be16 (pdb + 76, 3);
    be32 (pdb + 78, 104);
    be32 (pdb + 86, 424);
    be32 (pdb + 94, 429);
    mobi_header (pdb + 104, 6, 10, 0);
    be16 (pdb + 104, 2);
    be16 (pdb + 112, 2);
    be16 (pdb + 114, 5);
    memcpy (pdb + 424, "abc\200\033x", 6);
    assert (mulib_mobi_open (&r, pdb, 430) == 0);
    assert (mulib_mobi_text (&r, 10, &text) == MULIB_MOBI_EFORMAT
            && !text.length);
    be16 (pdb + 114, 9);
    assert (mulib_mobi_open (&r, pdb, 430) == 0);
    assert (mulib_mobi_text (&r, 10, &text) == 0 && text.length == 10
            && !memcmp (text.data, "abcabcabcx", 10));
    mulib_buffer_free (&text);
  }
}
static void
make_pdf (mulib_buffer_t *bytes)
{
  mulib_pdf_t doc = { 0 };
  size_t page = 42;
  assert (mulib_pdf_write (&doc, bytes) == MULIB_PDF_EINVAL);
  assert (mulib_pdf_add_page (&doc, NAN, 100, &page) == MULIB_PDF_EINVAL
          && !doc.count && page == 42);
  assert (mulib_pdf_add_page (&doc, 595, 842, &page) == 0 && page == 0);
  assert (mulib_pdf_text (&doc, 0, 72, 770, 12, "Hello (PDF) \\ world", 19)
          == 0);
  assert (mulib_pdf_text (&doc, 0, 0, 0, 12, "\xff", 1) == MULIB_PDF_EINVAL);
  assert (mulib_pdf_text (&doc, 0, NAN, 0, 12, "bad", 3) == MULIB_PDF_EINVAL);
  {
    size_t before = doc.pages[0].content.length;
    assert (mulib_pdf_color (&doc, 0, INFINITY, 0, 0) == MULIB_PDF_EINVAL);
    assert (mulib_pdf_text (&doc, 0, 0, 0, 0.0001, "bad", 3)
            == MULIB_PDF_EINVAL);
    assert (mulib_pdf_line (&doc, 0, -1000001, 0, 0, 0, 1)
            == MULIB_PDF_EINVAL);
    assert (mulib_pdf_rect (&doc, 0, 0, 0, -1, 0, 0, 0) == MULIB_PDF_EINVAL);
    assert (doc.pages[0].content.length == before);
  }
  assert (mulib_pdf_color (&doc, 0, 0.2, 0.4, 0.6) == 0);
  assert (mulib_pdf_rect (&doc, 0, 10, 20, 100, 50, 1, 0) == 0);
  assert (mulib_pdf_line (&doc, 0, -10, 0, 100, 200, 1) == 0);
  assert (mulib_pdf_add_page (&doc, 300, 400, &page) == 0 && page == 1);
  assert (mulib_pdf_text (&doc, 1, 20, 300, 24, "Second page", 11) == 0);
  assert (mulib_pdf_write (&doc, bytes) == 0);
  {
    size_t before = doc.pages[0].content.length, output = bytes->length;
    doc.pages[0].content.length = UINT32_MAX;
    assert (mulib_pdf_write (&doc, bytes) == MULIB_PDF_ELIMIT
            && bytes->length == output);
    doc.pages[0].content.length = before;
  }
  assert (mulib_pdf_write (&doc, bytes) == 0);
  assert (bytes->length % 2 == 0
          && !memcmp (bytes->data, bytes->data + bytes->length / 2,
                      bytes->length / 2));
  bytes->length /= 2;
  mulib_pdf_free (&doc);
  mulib_pdf_free (&doc);
}
static void
test_mathml (void)
{
  const char *valid
      = "<math "
        "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mfrac><mi>x</"
        "mi><msup><mi>y</mi><mn>2</mn></msup></mfrac></math>";
  const char *bad[] = {
    "<math/>",
    "<math "
    "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mfrac><mi>x</mi></mfrac></"
    "math>",
    "<math "
    "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mi><mn>1</mn></mi></math>",
    "<math "
    "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mtr><mtd/></mtr></math>",
    "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><mo "
    "stretchy=\"maybe\">+</mo></math>",
    "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><apply/></math>",
    "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">bare text</math>",
    "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><mtable/></math>",
    "<math "
    "xmlns=\"http://www.w3.org/1998/Math/MathML\"><msubsup><mi>x</mi><mn>1</"
    "mn></msubsup></math>",
    "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><mi "
    "xmlns=\"urn:other\">x</mi></math>"
  };
  mulib_mathml_error_t error;
  mulib_xml_node_t *doc, *copy, *math, *token;
  char *xml;
  size_t n, i;
  doc = mulib_mathml_parse (valid, strlen (valid), &error);
  assert (doc && !error.message);
  xml = mulib_mathml_serialize (doc, &n, &error);
  assert (xml && n == strlen (xml));
  copy = mulib_mathml_parse (xml, n, &error);
  assert (copy);
  free (xml);
  mulib_xml_destroy (copy);
  mulib_xml_destroy (doc);
  for (i = 0; i < sizeof (bad) / sizeof (bad[0]); ++i)
    {
      assert (!mulib_mathml_parse (bad[i], strlen (bad[i]), &error)
              && error.message && !error.node);
    }
  assert (!mulib_mathml_parse (NULL, 0, &error));
  math = mulib_mathml_new (1);
  token = mulib_mathml_token ("mi", "<&α");
  assert (math && token);
  assert (mulib_xml_append (math, token));
  xml = mulib_mathml_serialize (math, &n, &error);
  assert (xml && strstr (xml, "&lt;&amp;α"));
  free (xml);
  mulib_xml_destroy (math);
  assert (!mulib_mathml_token ("mrow", "x")
          && !mulib_mathml_element ("unsupported"));
  {
    const char *table = "<math "
                        "xmlns=\"http://www.w3.org/1998/Math/"
                        "MathML\"><mtable><mtr><mtd><mi>x</mi></"
                        "mtd><mtd><mn>2</mn></mtd></mtr></mtable></math>";
    mulib_xml_node_t *parent;
    doc = mulib_mathml_parse (table, strlen (table), &error);
    assert (doc);
    mulib_xml_destroy (doc);
    math = mulib_mathml_new (0);
    assert (math);
    parent = math;
    for (i = 1; i <= MULIB_XML_DEPTH_LIMIT; ++i)
      {
        token = mulib_mathml_element ("mrow");
        assert (token && mulib_xml_append (parent, token));
        parent = token;
        if (i == MULIB_XML_DEPTH_LIMIT - 1)
          assert (mulib_mathml_validate (math, &error));
      }
    n = 42;
    assert (!mulib_mathml_serialize (math, &n, &error) && !n
            && error.node == parent);
    mulib_xml_destroy (math);
    assert (!mulib_mathml_parse ("<!DOCTYPE math><math/>", 22, &error));
  }
}
static void
save (const char *path, const void *data, size_t n)
{
  FILE *f = fopen (path, "wb");
  assert (f);
  assert (fwrite (data, 1, n, f) == n);
  assert (fclose (f) == 0);
}
static void
load (const char *path, mulib_buffer_t *bytes)
{
  unsigned char chunk[4096];
  size_t n;
  FILE *f = fopen (path, "rb");
  assert (f);
  while ((n = fread (chunk, 1, sizeof (chunk), f)) != 0)
    assert (mulib_buffer_append (bytes, chunk, n));
  assert (!ferror (f) && fclose (f) == 0);
}
int
main (int argc, char **argv)
{
  mulib_buffer_t bytes = { 0 }, out = { 0 };
  if (argc == 4
      && (!strcmp (argv[1], "encode") || !strcmp (argv[1], "decode")))
    {
      int rc;
      load (argv[2], &bytes);
      rc = !strcmp (argv[1], "encode")
               ? mulib_deflate_encode (bytes.data, bytes.length, &out)
               : mulib_deflate_decode (bytes.data, bytes.length,
                                       4 * 1024 * 1024, &out);
      if (!rc)
        save (argv[3], out.data, out.length);
      mulib_buffer_free (&bytes);
      mulib_buffer_free (&out);
      return rc ? 1 : 0;
    }
  if (argc == 5 && !strcmp (argv[1], "unzip"))
    {
      mulib_zip_reader_t r = { 0 };
      const mulib_zip_entry_t *e;
      load (argv[2], &bytes);
      assert (mulib_zip_open (&r, bytes.data, bytes.length) == 0);
      e = mulib_zip_find (&r, argv[3]);
      assert (e);
      assert (mulib_zip_extract (&r, (size_t)(e - r.entries), 4 * 1024 * 1024,
                                 &out)
              == 0);
      save (argv[4], out.data, out.length);
      mulib_zip_close (&r);
      mulib_buffer_free (&bytes);
      mulib_buffer_free (&out);
      return 0;
    }
  if (argc == 3 && !strcmp (argv[1], "epub"))
    {
      mulib_epub_reader_t r = { 0 };
      int rc;
      load (argv[2], &bytes);
      rc = mulib_epub_open (&r, bytes.data, bytes.length, 65536);
      mulib_epub_close (&r);
      mulib_buffer_free (&bytes);
      return rc ? 1 : 0;
    }
  if (argc == 3 && !strcmp (argv[1], "emit"))
    {
      char path[4096];
      unsigned char pdb[2048];
      size_t size;
      static const char math[]
          = "<math "
            "xmlns=\"http://www.w3.org/1998/Math/MathML\"><mfrac><mn>1</"
            "mn><mi>x</mi></mfrac></math>";
      make_epub (&bytes);
      (void)snprintf (path, sizeof (path), "%s/book.epub", argv[2]);
      save (path, bytes.data, bytes.length);
      mulib_buffer_free (&bytes);
      make_pdf (&bytes);
      (void)snprintf (path, sizeof (path), "%s/book.pdf", argv[2]);
      save (path, bytes.data, bytes.length);
      mulib_buffer_free (&bytes);
      size = make_mobi (pdb, 6, 0);
      (void)snprintf (path, sizeof (path), "%s/book.mobi", argv[2]);
      save (path, pdb, size);
      size = make_mobi (pdb, 6, 1);
      (void)snprintf (path, sizeof (path), "%s/book.azw3", argv[2]);
      save (path, pdb, size);
      (void)snprintf (path, sizeof (path), "%s/math.xml", argv[2]);
      save (path, math, sizeof (math) - 1);
      assert (mulib_deflate_encode (chapter, sizeof (chapter) - 1, &bytes)
              == 0);
      (void)snprintf (path, sizeof (path), "%s/chapter.deflate", argv[2]);
      save (path, bytes.data, bytes.length);
      mulib_buffer_free (&bytes);
      return 0;
    }
  test_deflate ();
  test_zip ();
  test_epub ();
  test_mobi ();
  test_mathml ();
  make_pdf (&bytes);
  mulib_buffer_free (&bytes);
  puts ("ebook formats: passed");
  return 0;
}
