/* Never unregister a host agent: demonstrate the unopened-client failure. */
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_NM_SECRET_AGENT
#include "mulib-nm-secret-agent.h"
#include <assert.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  uint8_t reply[128];
  size_t n = 0;
  assert (mulib_nm_secret_agent_unregister (&c, reply, sizeof reply, &n)
          == -EINVAL);
  return 0;
}
