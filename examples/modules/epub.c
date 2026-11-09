#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_XML
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_EPUB
#include "mulib-epub.h"
#include <assert.h>
int
main (void)
{
  const char chapter[]
      = "<html "
        "xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Hello</title></"
        "head><body><p>Hello EPUB!</p></body></html>";
  mulib_epub_resource_t resource
      = { "chapter.xhtml", "application/xhtml+xml", "Hello", NULL,
          chapter,         sizeof (chapter) - 1 };
  mulib_epub_book_t book = { "urn:example:hello",    "Hello",   "en", "MuLIB",
                             "2026-11-09T00:00:00Z", &resource, 1 };
  mulib_buffer_t bytes = { 0 }, text = { 0 };
  mulib_epub_reader_t reader = { 0 };
  assert (mulib_epub_write (&book, &bytes) == 0);
  assert (mulib_epub_open (&reader, bytes.data, bytes.length, 65536) == 0);
  assert (reader.spine_count == 1);
  assert (mulib_epub_read (&reader, reader.spine[0], 65536, &text) == 0);
  assert (text.length == sizeof (chapter) - 1
          && !memcmp (text.data, chapter, text.length));
  mulib_epub_close (&reader);
  mulib_buffer_free (&text);
  mulib_buffer_free (&bytes);
  return 0;
}
