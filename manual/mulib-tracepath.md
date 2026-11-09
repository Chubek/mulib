# mulib-tracepath

**Single-hop UDP TTL probe with Linux IP_RECVERR error-queue response**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_TRACEPATH` once before including `mulib-tracepath.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_tracepath_probe(const uint8_t ipv4[4],uint16_t port,int ttl,int timeout_ms,int *icmp_type,int *icmp_code);
```

## Semantics and safety

One IPv4 UDP TTL probe per call. Linux error-queue only; not a full multi-hop traceroute orchestrator. Does not require raw socket privilege. Caller interprets ICMP type/code.

## Demonstration

See `../tests/test_tracepath.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tracepath.h`](../include/mulib-tracepath.h) |
| Implementation switch | `MULIB_IMPL_TRACEPATH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/tracepath.c`](../examples/modules/tracepath.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tracepath.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
