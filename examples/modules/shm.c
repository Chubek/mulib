#define MULIB_IMPL_SHM
#include "mulib-shm.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
#if defined(__unix__) || defined(__APPLE__)
  mulib_shm_t a = MULIB_SHM_INIT, b = MULIB_SHM_INIT;
  assert (!mulib_shm_create (&a, 4096));
  assert (!mulib_shm_attach (&b, a.id, 1));
  memcpy (a.data, "shared", 7);
  assert (!memcmp (b.data, "shared", 7));
  assert (!mulib_shm_remove (a.id));
  assert (!mulib_shm_close (&b));
  assert (!mulib_shm_close (&a));
#endif
  return 0;
}
