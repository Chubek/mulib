# mulib-networkd-client

**Read systemd-networkd managed link list as a raw D-Bus reply**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETWORKD_CLIENT` once before including `mulib-networkd-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-client.h`.

## Public API

```c
int mulib_networkd_client_links(mulib_dbus_client_t *c,void *reply,size_t cap,size_t *written);
```

## Semantics and safety

Read-only ListLinks. May not be available on systems without systemd-networkd; caller decodes raw return data.

## Demonstration

See `../tests/test_networkd-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-networkd-client.h`](../include/mulib-networkd-client.h) |
| Implementation switch | `MULIB_IMPL_NETWORKD_CLIENT` |
| Repository-local includes | [mulib-dbus-client](mulib-dbus-client.md) |
| Complete example | [`examples/modules/networkd-client.c`](../examples/modules/networkd-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/networkd-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
