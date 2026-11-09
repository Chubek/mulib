# mulib-dbus-properties

**Call D-Bus Properties.Get and decode uint32 or string variant replies**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DBUS_PROPERTIES` once before including `mulib-dbus-properties.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-client.h`.

## Public API

```c
int mulib_dbus_properties_u32(mulib_dbus_client_t *client,const char *service,
                              const char *path,const char *interface,const char *property,uint32_t *value);
int mulib_dbus_properties_string(mulib_dbus_client_t *client,const char *service,
                                 const char *path,const char *interface,const char *property,char *out,size_t capacity);
```

## Semantics and safety

Decoding is bounded by the declared body, not trailing bytes in the supplied
buffer. Truncated or unterminated variants return `-EBADMSG`; a valid string
that exceeds the caller's output capacity returns `-ENOSPC`. Outputs are
published only on successful extraction. Only `u` and `s` variants are supported.

Parses a narrow D-Bus variant layout containing uint32 or string only. Does not support arbitrary arrays/dictionaries/object paths or every basic D-Bus type. Get property uses two string arguments.

## Demonstration

See `../tests/test_dbus-properties.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dbus-properties.h`](../include/mulib-dbus-properties.h) |
| Implementation switch | `MULIB_IMPL_DBUS_PROPERTIES` |
| Repository-local includes | [mulib-dbus-client](mulib-dbus-client.md) |
| Complete example | [`examples/modules/dbus-properties.c`](../examples/modules/dbus-properties.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbus-properties.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
