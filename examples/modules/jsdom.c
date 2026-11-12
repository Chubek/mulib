/* Complete, offline jsdom example. */
#define MULIB_IMPL_JSDOM
#include "mulib-jsdom.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
dispatch (void *ctx, mulib_jsdom_op_t op, uint64_t id, const char *property,
          const char *value, size_t n, char *out, size_t cap)
{
  (void)ctx;
  (void)op;
  (void)id;
  (void)property;
  (void)value;
  (void)n;
  if (cap < 6)
    return 0;
  memcpy (out, "hello", 6);
  return 1;
}

int
main (void)
{
  mulib_jsdom_t bridge = { dispatch, NULL };
  char result[32];
  assert (mulib_jsdom_invoke (&bridge, MULIB_JSDOM_GET, 1, "title", NULL, 0,
                              result, sizeof result));
  assert (!strcmp (result, "hello"));
  return 0;
}
