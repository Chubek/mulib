#define MULIB_IMPL_OBJFILEPACKER
#define MULIB_IMPL_BUFFER
#include "mulib-objfilepacker.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t ar = { 0 };
  assert (mulib_objfilepacker_add (&ar, "data.o", "abc", 3));
  assert (ar.length == 72 && ar.data[0] == '!');
  mulib_buffer_free (&ar);
  return 0;
}
