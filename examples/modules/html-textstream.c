/* Complete, offline html-textstream example. */
#define MULIB_IMPL_HTML_TEXTSTREAM
#include "mulib-html-textstream.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
on_html (void *ctx, const mulib_html_textstream_event_t *e)
{
  (void)e;
  (*(size_t *)ctx)++;
  return 1;
}

int
main (void)
{
  const char *html = "<p>Hello</p>";
  size_t count = 0;
  assert (
      mulib_html_textstream_scan (html, strlen (html), on_html, &count, 100));
  assert (count == 3);
  return 0;
}
