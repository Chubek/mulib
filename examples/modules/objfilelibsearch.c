#define MULIB_IMPL_OBJFILELIBSEARCH
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_OBJFILEPACKER
#define MULIB_IMPL_BUFFER
#include "mulib-objfilelibsearch.h"
#include "mulib-objfilepacker.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t ar = { 0 };
  mulib_binfile_t member;
  assert (mulib_objfilepacker_add (&ar, "data.o", "abc", 3));
  assert (!mulib_objfilelibsearch_find (ar.data, ar.length, "data.o", &member)
          && member.size == 3);
  mulib_buffer_free (&ar);
  return 0;
}
