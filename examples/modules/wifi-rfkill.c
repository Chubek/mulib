#define MULIB_IMPL_WIFI_RFKILL
#include "mulib-wifi-rfkill.h"
#include <stdio.h>
int
main (void)
{
  int fd = mulib_wifi_rfkill_open ();
  if (fd >= 0)
    {
      mulib_wifi_rfkill_state_t state;
      printf ("rfkill read status=%d\n", mulib_wifi_rfkill_read (fd, &state));
      close (fd);
    }
  else
    printf ("rfkill open status=%d\n", fd);
  return 0;
}
