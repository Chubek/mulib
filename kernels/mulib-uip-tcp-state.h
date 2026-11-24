/** @file mulib-uip-tcp-state.h
 * @brief Deterministic subset TCP connection state transitions for active
 * open/close C11, single-header, define MULIB_IMPL_UIP_TCP_STATE exactly once.
 */
#ifndef MULIB_UIP_TCP_STATE_H
#define MULIB_UIP_TCP_STATE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_uip_tcp_state
  {
    MULIB_UIP_TCP_CLOSED = 0,
    MULIB_UIP_TCP_LISTEN,
    MULIB_UIP_TCP_SYN_SENT,
    MULIB_UIP_TCP_SYN_RCVD,
    MULIB_UIP_TCP_ESTABLISHED,
    MULIB_UIP_TCP_FIN_WAIT1,
    MULIB_UIP_TCP_FIN_WAIT2,
    MULIB_UIP_TCP_CLOSE_WAIT,
    MULIB_UIP_TCP_LAST_ACK,
    MULIB_UIP_TCP_TIME_WAIT
  } mulib_uip_tcp_state_t;
  typedef enum mulib_uip_tcp_event
  {
    MULIB_UIP_TCP_LISTEN_EVENT = 1,
    MULIB_UIP_TCP_ACTIVE_OPEN,
    MULIB_UIP_TCP_SYN,
    MULIB_UIP_TCP_SYN_ACK,
    MULIB_UIP_TCP_ACK,
    MULIB_UIP_TCP_FIN,
    MULIB_UIP_TCP_CLOSE,
    MULIB_UIP_TCP_TIMEOUT,
    MULIB_UIP_TCP_RST
  } mulib_uip_tcp_event_t;
  int mulib_uip_tcp_state_next (mulib_uip_tcp_state_t *state,
                                mulib_uip_tcp_event_t event);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_TCP_STATE)                                         \
    && !defined(MULIB_UIP_TCP_STATE_IMPLEMENTED)
#define MULIB_UIP_TCP_STATE_IMPLEMENTED
#include <errno.h>
int
mulib_uip_tcp_state_next (mulib_uip_tcp_state_t *s, mulib_uip_tcp_event_t e)
{
  if (!s)
    return -EINVAL;
  if (e == MULIB_UIP_TCP_RST)
    {
      *s = MULIB_UIP_TCP_CLOSED;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_CLOSED && e == MULIB_UIP_TCP_LISTEN_EVENT)
    {
      *s = MULIB_UIP_TCP_LISTEN;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_CLOSED && e == MULIB_UIP_TCP_ACTIVE_OPEN)
    {
      *s = MULIB_UIP_TCP_SYN_SENT;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_LISTEN && e == MULIB_UIP_TCP_SYN)
    {
      *s = MULIB_UIP_TCP_SYN_RCVD;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_SYN_SENT && e == MULIB_UIP_TCP_SYN_ACK)
    {
      *s = MULIB_UIP_TCP_ESTABLISHED;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_SYN_RCVD && e == MULIB_UIP_TCP_ACK)
    {
      *s = MULIB_UIP_TCP_ESTABLISHED;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_ESTABLISHED && e == MULIB_UIP_TCP_FIN)
    {
      *s = MULIB_UIP_TCP_CLOSE_WAIT;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_ESTABLISHED && e == MULIB_UIP_TCP_CLOSE)
    {
      *s = MULIB_UIP_TCP_FIN_WAIT1;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_FIN_WAIT1 && e == MULIB_UIP_TCP_ACK)
    {
      *s = MULIB_UIP_TCP_FIN_WAIT2;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_FIN_WAIT2 && e == MULIB_UIP_TCP_FIN)
    {
      *s = MULIB_UIP_TCP_TIME_WAIT;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_CLOSE_WAIT && e == MULIB_UIP_TCP_CLOSE)
    {
      *s = MULIB_UIP_TCP_LAST_ACK;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_LAST_ACK && e == MULIB_UIP_TCP_ACK)
    {
      *s = MULIB_UIP_TCP_CLOSED;
      return 0;
    }
  if (*s == MULIB_UIP_TCP_TIME_WAIT && e == MULIB_UIP_TCP_TIMEOUT)
    {
      *s = MULIB_UIP_TCP_CLOSED;
      return 0;
    }
  return -EINVAL;
}
#endif
