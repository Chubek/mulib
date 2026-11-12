/* Complete, offline browser-resource example. */
#define MULIB_IMPL_BROWSER_RESOURCE
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-browser-resource.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_resources_t r = { 0 };
  size_t id;
  assert (mulib_browser_resource_register (&r, "https://example.com", &id));
  assert (mulib_browser_resource_complete (&r, id, 200, "text/html", 12));
  mulib_browser_resources_free (&r);
  return 0;
}
