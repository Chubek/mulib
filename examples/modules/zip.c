#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#include "mulib-zip.h"
#include <assert.h>
int
main (void)
{
  mulib_zip_writer_t writer = { 0 };
  mulib_zip_reader_t reader = { 0 };
  mulib_buffer_t bytes = { 0 };
  assert (mulib_zip_add (&writer, "hello.txt", "Hello ZIP", 9, 8) == 0);
  assert (mulib_zip_finish (&writer) == 0);
  assert (mulib_zip_open (&reader, writer.bytes.data, writer.bytes.length)
          == 0);
  assert (mulib_zip_find (&reader, "hello.txt"));
  assert (mulib_zip_extract (&reader, 0, 100, &bytes) == 0);
  assert (bytes.length == 9 && !memcmp (bytes.data, "Hello ZIP", 9));
  mulib_buffer_free (&bytes);
  mulib_zip_close (&reader);
  mulib_zip_writer_free (&writer);
  return 0;
}
