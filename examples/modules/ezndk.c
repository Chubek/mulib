#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_EZNDK
#include "mulib-ezndk.h"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  mulib_ezndk_window_t window = {0};
  assert (mulib_ezndk_window_close (&window) == MULIB_EZNDK_OK);
  if (mulib_ezndk_available ())
    assert (mulib_ezndk_log (4,"MuLIB","Literal messages are not printf formats: %s") == MULIB_EZNDK_OK);
  else
    assert (mulib_ezndk_log (4,"MuLIB","SDK-free build") == MULIB_EZNDK_UNSUPPORTED);
  puts ("NDK backend explicitly detected; no fake assets or windows."); return 0;
}
