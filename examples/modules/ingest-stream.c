#define MULIB_IMPL_INGEST_STREAM
#include "mulib-ingest-stream.h"
#include <assert.h>
#include <stdio.h>
static int
record (const void *bytes, size_t n, void *ctx)
{
  ++*(size_t *)ctx;
  if (n && fwrite (bytes, 1, n, stdout) != n)
    return 0;
  return putchar ('\n') != EOF;
}
int
main (void)
{
  mulib_ingest_stream_t stream = { 0 };
  size_t count = 0;
  assert (mulib_ingest_stream_init (&stream, 1024));
  assert (mulib_ingest_stream_push (&stream, "hel", 3, record, &count) == 0);
  assert (mulib_ingest_stream_push (&stream, "lo\nworld", 8, record, &count) == 1);
  assert (mulib_ingest_stream_finish (&stream, record, &count) == 1 && count == 2);
  mulib_ingest_stream_free (&stream);
  return 0;
}
