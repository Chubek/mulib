/* Complete, offline readability example. */
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_READABILITY
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-readability.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_shylock_buffer_t text = { 0 };
  const char *html = "<p>A &amp; B</p><style>hidden</style>";
  assert (mulib_readability_extract (html, strlen (html), &text));
  assert (!strcmp (text.data, "A & B"));
  mulib_shylock_buffer_free (&text);
  return 0;
}
