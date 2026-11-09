/* Validate the interface before any control command is sent. */
#define MULIB_IMPL_WPA_CTRL
#include "mulib-wpa-ctrl.h"
#include <assert.h>
int
main (void)
{
  char reply[128];
  size_t n = 0;
  assert (
      mulib_wpa_ctrl_request ("../invalid", "PING", reply, sizeof reply, &n)
      == -EINVAL);
  return 0;
}
