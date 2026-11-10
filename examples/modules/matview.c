#define MULIB_IMPL_MATVIEW
#include "mulib-matview.h"
#include <assert.h>
int
main (void)
{
  mulib_matview_t view = { 0 };
  assert (mulib_matview_insert (&view, 10) && mulib_matview_insert (&view, 20));
  assert (mulib_matview_avg (&view) == 15 && mulib_matview_delete (&view, 10));
  assert (mulib_matview_avg (&view) == 20);
  return 0;
}
