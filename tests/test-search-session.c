#define MULIB_IMPL_SEARCH_SESSION
#include "mulib-search-session.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_search_session_t s;
  assert (mulib_search_session_init (&s, 10, 15));
  assert (mulib_search_session_next (&s) == 10);
  assert (mulib_search_session_next (&s) == 5);
  return 0;
}
