#define MULIB_IMPL_MULIB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#include "mulib-mulib.h"
#include <assert.h>
int
main (void)
{
  assert (!strcmp (mulib_version (), MULIB_VERSION_STRING));
  return 0;
}
