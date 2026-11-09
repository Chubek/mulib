# mulib-ustack-tuntap

**Drive userspace Ethernet stack using an existing Linux TAP descriptor**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_USTACK_TUNTAP` once before including `mulib-ustack-tuntap.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-ustack.h`.

## Public API

```c
int mulib_ustack_tap_once(int tap_fd,mulib_ustack_t *stack);
int mulib_ustack_tap_emit(const void *frame,size_t n,void *context);
```

## Semantics and safety

Operates on an already open **TAP** fd (Ethernet frames). Not for TUN raw IP packets. No device creation/permissions, no polling or backpressure beyond one blocking read/write; driver loop controlled by caller. Can use existing mulib-tuntap.h to create TAP device.

## Demonstration

See `../tests/test_ustack-tuntap.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ustack-tuntap.h`](../include/mulib-ustack-tuntap.h) |
| Implementation switch | `MULIB_IMPL_USTACK_TUNTAP` |
| Repository-local includes | [mulib-ustack](mulib-ustack.md) |
| Complete example | [`examples/modules/ustack-tuntap.c`](../examples/modules/ustack-tuntap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ustack-tuntap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
