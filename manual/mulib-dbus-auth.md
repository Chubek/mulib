# mulib-dbus-auth

**Authenticate to the system D-Bus socket using EXTERNAL peer credentials**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DBUS_AUTH` once before including `mulib-dbus-auth.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_dbus_auth_external(int fd);
```

## Semantics and safety

Successful authentication requires an entire `OK <32 hex digits>\r\n` line.
Truncated, oversized or malformed replies fail with `-EACCES`; `BEGIN` is not
sent after failure. The descriptor is borrowed. Direct users must arrange
socket timeouts; `mulib_dbus_transport_connect` installs them for its own socket.

Linux system bus EXTERNAL SASL with effective-UID hexadecimal identity; only unencrypted local UNIX sockets. No FD-passing negotiation, anonymous auth or cookie auth. EXTERNAL is peer-credential based; do not treat it as a remote authentication protocol.

## Demonstration

See `../tests/test_dbus-auth.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dbus-auth.h`](../include/mulib-dbus-auth.h) |
| Implementation switch | `MULIB_IMPL_DBUS_AUTH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dbus-auth.c`](../examples/modules/dbus-auth.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbus-auth.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
