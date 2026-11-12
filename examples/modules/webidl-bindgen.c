/* Complete, offline webidl-bindgen example. */
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_WEBIDL_BINDGEN
#include "mulib-webidl-bindgen.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_shylock_buffer_t header = { 0 };
  const char *idl = "interface Node { attribute DOMString name; };";
  assert (mulib_webidl_bindgen_interfaces (idl, strlen (idl), &header, 10));
  assert (strstr (header.data, "mulib_webidl_Node"));
  mulib_shylock_buffer_free (&header);
  return 0;
}
