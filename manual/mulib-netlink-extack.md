# mulib-netlink-extack

**Decode NLMSG_ERROR and kernel extended acknowledgment attributes**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETLINK_EXTACK` once before including `mulib-netlink-extack.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
typedef struct mulib_netlink_error {int code;uint32_t offset;char message[256];} mulib_netlink_error_t;
int mulib_netlink_extack_parse(const void *frame,size_t length,mulib_netlink_error_t *out);
```

## Semantics and safety

Attributes are decoded only with `NLM_F_ACK_TLVS`. For uncapped negative errors,
the aligned echoed request payload is skipped before reading attributes; an
incomplete echo is rejected. Message text must be NUL-terminated, is truncated
to 255 characters, and remains terminated. Malformed known attributes return
`-EBADMSG` without publishing a partial result. Unaligned frames are accepted.

Supports error code, extack offset and message. No JSON or arbitrary nested extack. Only valid NLMSG_ERROR frames accepted.

## Demonstration

See `../tests/test_netlink-extack.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netlink-extack.h`](../include/mulib-netlink-extack.h) |
| Implementation switch | `MULIB_IMPL_NETLINK_EXTACK` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/netlink-extack.c`](../examples/modules/netlink-extack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netlink-extack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
