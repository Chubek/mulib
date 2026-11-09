/* Focused, memory-only fuzzing for publication parsers and their codecs. */
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
#define MULIB_IMPL_MATHML
#include "mulib-azw3.h"
#include "mulib-epub.h"
#include "mulib-mathml.h"
#include <assert.h>

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  mulib_buffer_t out = { 0 };
  mulib_zip_reader_t zip = { 0 };
  mulib_epub_reader_t epub = { 0 };
  mulib_mobi_reader_t mobi;
  mulib_azw3_reader_t azw3;
  mulib_xml_node_t *math;
  size_t i;
  if (size > 65536)
    return 0;
  (void)mulib_deflate_decode (data, size, 65536, &out);
  assert (out.length <= 65536);
  mulib_buffer_free (&out);
  (void)mulib_mobi_palmdoc_decode (data, size, 65536, &out);
  assert (out.length <= 65536);
  mulib_buffer_free (&out);
  if (mulib_zip_open (&zip, data, size) == 0)
    {
      for (i = 0; i < zip.count && i < 8; ++i)
        {
          (void)mulib_zip_extract (&zip, i, 65536, &out);
          assert (out.length <= 65536);
          mulib_buffer_free (&out);
        }
    }
  mulib_zip_close (&zip);
  if (mulib_epub_open (&epub, data, size, 65536) == 0 && epub.spine_count)
    {
      (void)mulib_epub_read (&epub, epub.spine[0], 65536, &out);
      assert (out.length <= 65536);
      mulib_buffer_free (&out);
    }
  mulib_epub_close (&epub);
  if (mulib_mobi_open (&mobi, data, size) == 0)
    {
      (void)mulib_mobi_text (&mobi, 65536, &out);
      assert (out.length <= 65536);
      mulib_buffer_free (&out);
    }
  if (mulib_azw3_open (&azw3, data, size) == 0)
    {
      (void)mulib_azw3_flow (&azw3, 0, 65536, &out);
      assert (out.length <= 65536);
      mulib_buffer_free (&out);
    }
  math = mulib_mathml_parse ((const char *)data, size, NULL);
  if (math)
    {
      size_t length;
      char *serialized = mulib_mathml_serialize (math, &length, NULL);
      if (serialized)
        {
          mulib_xml_node_t *copy
              = mulib_mathml_parse (serialized, length, NULL);
          assert (copy);
          mulib_xml_destroy (copy);
          free (serialized);
        }
      mulib_xml_destroy (math);
    }
  return 0;
}
