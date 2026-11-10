#define MULIB_IMPL_NDJSON
#include "mulib-ndjson.h"
#include <assert.h>
int
main (void)
{
  const char input[] = "{}\r\n\n[]";
  mulib_ndjson_t p;
  const char *line;
  size_t n;
  mulib_ndjson_init (&p, input, sizeof (input) - 1);
  assert (mulib_ndjson_next (&p, &line, &n) == 1 && n == 2 && !memcmp (line, "{}", 2));
  assert (mulib_ndjson_next (&p, &line, &n) == 1 && n == 2 && !memcmp (line, "[]", 2));
  assert (!mulib_ndjson_next (&p, &line, &n));
  return 0;
}
