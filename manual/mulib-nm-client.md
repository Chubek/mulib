# mulib-nm-client

**Read NetworkManager global state and device object-path reply**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NM_CLIENT` once before including `mulib-nm-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-properties.h`.

## Public API

```c
int mulib_nm_client_state(mulib_dbus_client_t *c,uint32_t *state);
int mulib_nm_client_devices(mulib_dbus_client_t *c,void *reply,size_t cap,size_t *used);
```

## Semantics and safety

Read-only GetDevices returns raw D-Bus array of object paths, not decoded list. State returns one u32 property. Does not activate connections or parse NetworkManager settings dictionaries.

## Demonstration

See `../tests/test_nm-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-nm-client.h`](../include/mulib-nm-client.h) |
| Implementation switch | `MULIB_IMPL_NM_CLIENT` |
| Repository-local includes | [mulib-dbus-properties](mulib-dbus-properties.md) |
| Complete example | [`examples/modules/nm-client.c`](../examples/modules/nm-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nm-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
