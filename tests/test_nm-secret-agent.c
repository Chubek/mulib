/* Generated, standalone MuLIB add-on symbol and API test: nm-secret-agent */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_NM_SECRET_AGENT
#include "mulib-nm-secret-agent.h"
int
main (void)
{
  {
    void (*volatile p) (void)
        = (void (*) (void))&mulib_nm_secret_agent_unregister;
    assert (p);
  }
  puts ("PASS nm-secret-agent");
  return 0;
}
