# mulib-dbus-transport

**Connect to and read/write a framed D-Bus UNIX system-bus connection**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DBUS_TRANSPORT` once before including `mulib-dbus-transport.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-auth.h`.

## Public API

```c
int mulib_dbus_transport_connect(const char *path);
int mulib_dbus_transport_write(int fd,const void *bytes,size_t count);
int mulib_dbus_transport_read(int fd,void *bytes,size_t capacity,size_t *count);
```

## Semantics and safety

Only little-endian D-Bus version 1 frames are supported, with a nonzero serial,
message types 1 through 4, and a total aligned frame limit of 1 MiB. Lengths are
decoded explicitly, independent of host endianness. A short/invalid frame or a
buffer-capacity failure can leave the stream partially consumed: close it
instead of attempting to read another frame. This transport does not negotiate
UNIX FD passing.

D-Bus protocol version 1, little-endian messages only, max 1 MiB read buffer. No descriptor passing and no bus address parser. Failure to read oversize frame leaves stream unsynchronized; caller must close fd.

## Demonstration

See `../tests/test_dbus-transport.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dbus-transport.h`](../include/mulib-dbus-transport.h) |
| Implementation switch | `MULIB_IMPL_DBUS_TRANSPORT` |
| Repository-local includes | [mulib-dbus-auth](mulib-dbus-auth.md) |
| Complete example | [`examples/modules/dbus-transport.c`](../examples/modules/dbus-transport.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbus-transport.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
