#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SSE
#include "mulib-sse.h"
#include <stdio.h>
static int event (void *user, const mulib_sse_event_t *e)
{
  (void)user; printf ("%s: ", e->event);
  return fwrite (e->data, 1, e->data_length, stdout) == e->data_length && putchar ('\n') != EOF;
}
int main (void)
{
  const char wire[] = "event: message\ndata: hello\n\n";
  mulib_sse_parser_t *p = mulib_sse_create (4096, event, NULL);
  int rc;
  if (!p) return 1;
  rc = mulib_sse_feed (p, wire, sizeof wire - 1);
  if (rc == 1) rc = mulib_sse_finish (p);
  mulib_sse_destroy (p); return rc == 1 ? 0 : 1;
}
