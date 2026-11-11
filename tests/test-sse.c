#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SSE
#include "mulib-sse.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int event (void *user, const mulib_sse_event_t *e)
{
  unsigned *count = (unsigned *)user;
  ++*count;
  assert (!strcmp (e->id, "42"));
  if (*count == 1) {
    assert (!strcmp (e->event, "token"));
    assert (e->data_length == 6 && !memcmp (e->data, "h\xc3\xa9\nx!", 6));
  } else assert (!strcmp (e->event, "message") && e->data_length == 0);
  return 1;
}
int main (void)
{
  const char wire[] = "\xef\xbb\xbf: comment\r\nid: 42\revent: token\ndata: h\xc3\xa9\r\ndata:x!\n\ndata:\n\ndata: discarded";
  for (size_t step = 1; step < sizeof wire; ++step) {
    unsigned count = 0;
    mulib_sse_parser_t *s = mulib_sse_create (128, event, &count); assert (s);
    for (size_t i = 0; i < sizeof wire - 1;) {
      size_t n = sizeof wire - 1 - i; if (n > step) n = step;
      assert (mulib_sse_feed (s, wire + i, n) == 1); i += n;
    }
    assert (mulib_sse_finish (s) == 1 && count == 2);
    mulib_sse_destroy (s);
  }
  {
    unsigned count = 0;
    mulib_sse_parser_t *s = mulib_sse_create (4, event, &count);
    assert (mulib_sse_feed (s, "12345", 5) == -2);
    assert (mulib_sse_finish (s) == -2); mulib_sse_destroy (s);
    s = mulib_sse_create (128, event, &count);
    assert (mulib_sse_feed (s, "data: \xff\n", 8) == -1); mulib_sse_destroy (s);
  }
  {
    mulib_sse_event_t e = {"token", "42", "h\xc3\xa9\nx!", 6};
    char *out = NULL; size_t n = 0; unsigned count = 0;
    mulib_sse_parser_t *s = mulib_sse_create (128, event, &count);
    assert (mulib_sse_encode (&e, 128, &out, &n) == 1);
    assert (mulib_sse_feed (s, out, n) == 1 && count == 1);
    free (out); mulib_sse_destroy (s);
    e.event = "bad\nevent"; assert (mulib_sse_encode (&e, 128, &out, &n) == -1);
  }
  return 0;
}
