#define MULIB_IMPL_UIP_TCP_STATE
#include "mulib-uip-tcp-state.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_tcp_state_t state = MULIB_UIP_TCP_CLOSED;
  assert (!mulib_uip_tcp_state_next (&state, MULIB_UIP_TCP_ACTIVE_OPEN));
  assert (!mulib_uip_tcp_state_next (&state, MULIB_UIP_TCP_SYN_ACK));
  assert (state == MULIB_UIP_TCP_ESTABLISHED);
  return 0;
}
