#define MULIB_IMPL_MQUEUE
#include "mulib-mqueue.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
#if defined(__unix__) || defined(__APPLE__)
  int id;
  long type;
  char bytes[16];
  size_t n;
  assert (!mulib_mqueue_create (&id));
  assert (!mulib_mqueue_send (id, 2, "message", 7));
  assert (!mulib_mqueue_receive (id, 0, &type, bytes, sizeof bytes, &n));
  assert (type == 2 && n == 7 && !memcmp (bytes, "message", 7));
  assert (!mulib_mqueue_remove (id));
#endif
  return 0;
}
