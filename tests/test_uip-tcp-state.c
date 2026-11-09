/* Generated, standalone MuLIB add-on symbol and API test: uip-tcp-state */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_TCP_STATE
#include "mulib-uip-tcp-state.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_tcp_state_next;
    assert (p);
  }

  mulib_uip_tcp_state_t s = MULIB_UIP_TCP_CLOSED;
  assert (!mulib_uip_tcp_state_next (&s, MULIB_UIP_TCP_ACTIVE_OPEN));
  assert (s == MULIB_UIP_TCP_SYN_SENT);
  assert (!mulib_uip_tcp_state_next (&s, MULIB_UIP_TCP_SYN_ACK));
  assert (s == MULIB_UIP_TCP_ESTABLISHED);
  puts ("PASS uip-tcp-state");
  return 0;
}
