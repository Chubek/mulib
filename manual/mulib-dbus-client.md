# mulib-dbus-client

**Minimal system D-Bus method-call client with zero, one, or two string arguments**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DBUS_CLIENT` once before including `mulib-dbus-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-transport.h`.

## Public API

```c
typedef struct mulib_dbus_client {int fd;uint32_t serial;} mulib_dbus_client_t;
#define MULIB_DBUS_CLIENT_INIT {-1,0}
int mulib_dbus_client_open(mulib_dbus_client_t *c,const char *system_bus_path);
int mulib_dbus_client_close(mulib_dbus_client_t *c);
int mulib_dbus_client_call(mulib_dbus_client_t *c,const char *destination,const char *object_path,
                           const char *interface,const char *member,const char *arg1,const char *arg2,
                           void *response,size_t capacity,size_t *received);
size_t mulib_dbus_client_body_offset(const void *message,size_t size);
```

## Semantics and safety

Serial zero is skipped on wraparound. Reply fields require single-type,
NUL-terminated variants, correctly typed standard fields, unique standard field
codes, and terminated strings. Replies/errors require a nonzero reply serial;
unrelated valid frames are skipped (up to the documented iteration budget).
Malformed framing returns `-EBADMSG`; a matching remote error returns
`-EREMOTEIO`. The supported header value types are `u`, `s`, `o`, and `g`;
other variant types return `-EPROTONOSUPPORT`. Close the connection after a
framing/transport error. Calls are synchronous and must not share a connection
concurrently. This focused client is not a full D-Bus validator or dispatcher.

Linux UNIX socket system bus only. Method calls with no arguments or one/two D-Bus strings only; bounded replies up to caller capacity, max 64 skipped signals, 3-s socket I/O timeout. Raw reply returned to caller. No concurrent calls, no UNIX_FD, no variant-argument encoder, no signal subscription. Suitable for simple read-only service methods, not full NetworkManager configuration.

## Demonstration

See `../tests/test_dbus-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dbus-client.h`](../include/mulib-dbus-client.h) |
| Implementation switch | `MULIB_IMPL_DBUS_CLIENT` |
| Repository-local includes | [mulib-dbus-transport](mulib-dbus-transport.md) |
| Complete example | [`examples/modules/dbus-client.c`](../examples/modules/dbus-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbus-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
