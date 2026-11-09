# mulib-polkit-client

**Check whether the PolicyKit D-Bus service is currently registered**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_POLKIT_CLIENT` once before including `mulib-polkit-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-client.h`.

## Public API

```c
int mulib_polkit_client_name_has_owner(mulib_dbus_client_t *c,int *present);
```

## Semantics and safety

D-Bus NameHasOwner availability check only. Does NOT implement polkit CheckAuthorization, agent authentication, or elevation. Never treat service presence as proof of authorization.

## Demonstration

See `../tests/test_polkit-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-polkit-client.h`](../include/mulib-polkit-client.h) |
| Implementation switch | `MULIB_IMPL_POLKIT_CLIENT` |
| Repository-local includes | [mulib-dbus-client](mulib-dbus-client.md) |
| Complete example | [`examples/modules/polkit-client.c`](../examples/modules/polkit-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/polkit-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
