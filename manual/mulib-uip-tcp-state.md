# mulib-uip-tcp-state

**Deterministic subset TCP connection state transitions for active open/close**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_TCP_STATE` once before including `mulib-uip-tcp-state.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef enum mulib_uip_tcp_state {MULIB_UIP_TCP_CLOSED=0,MULIB_UIP_TCP_LISTEN,MULIB_UIP_TCP_SYN_SENT,MULIB_UIP_TCP_SYN_RCVD,MULIB_UIP_TCP_ESTABLISHED,MULIB_UIP_TCP_FIN_WAIT1,MULIB_UIP_TCP_FIN_WAIT2,MULIB_UIP_TCP_CLOSE_WAIT,MULIB_UIP_TCP_LAST_ACK,MULIB_UIP_TCP_TIME_WAIT} mulib_uip_tcp_state_t;
typedef enum mulib_uip_tcp_event {MULIB_UIP_TCP_LISTEN_EVENT=1,MULIB_UIP_TCP_ACTIVE_OPEN,MULIB_UIP_TCP_SYN,MULIB_UIP_TCP_SYN_ACK,MULIB_UIP_TCP_ACK,MULIB_UIP_TCP_FIN,MULIB_UIP_TCP_CLOSE,MULIB_UIP_TCP_TIMEOUT,MULIB_UIP_TCP_RST} mulib_uip_tcp_event_t;
int mulib_uip_tcp_state_next(mulib_uip_tcp_state_t *state,mulib_uip_tcp_event_t event);
```

## Semantics and safety

Illustrative finite state machine only, not actual end-to-end TCP implementation. No timers, retransmit, sequence validation or simultaneous open; invalid events return -EINVAL.

## Demonstration

See `../tests/test_uip-tcp-state.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-tcp-state.h`](../include/mulib-uip-tcp-state.h) |
| Implementation switch | `MULIB_IMPL_UIP_TCP_STATE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-tcp-state.c`](../examples/modules/uip-tcp-state.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-tcp-state.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
