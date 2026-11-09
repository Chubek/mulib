# mulib-nm-device

**Read a NetworkManager device Interface string via D-Bus Properties.Get**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NM_DEVICE` once before including `mulib-nm-device.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-properties.h`.

## Public API

```c
int mulib_nm_device_interface(mulib_dbus_client_t *c,const char *device_path,char *out,size_t cap);
```

## Semantics and safety

One property getter, expects NetworkManager device path. Not connection activation, Wi-Fi control, state caching, or live property notifications.

## Demonstration

See `../tests/test_nm-device.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-nm-device.h`](../include/mulib-nm-device.h) |
| Implementation switch | `MULIB_IMPL_NM_DEVICE` |
| Repository-local includes | [mulib-dbus-properties](mulib-dbus-properties.md) |
| Complete example | [`examples/modules/nm-device.c`](../examples/modules/nm-device.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nm-device.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
