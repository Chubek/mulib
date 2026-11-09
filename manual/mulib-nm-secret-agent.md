# mulib-nm-secret-agent

**Explicitly unregister a NetworkManager secret agent already registered by the caller**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NM_SECRET_AGENT` once before including `mulib-nm-secret-agent.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-dbus-client.h`.

## Public API

```c
int mulib_nm_secret_agent_unregister(mulib_dbus_client_t *c,void *reply,size_t cap,size_t *written);
```

## Semantics and safety

Only Unregister (safe cleanup) for a previously registered agent. Does NOT register, expose GetSecrets callbacks, store secrets, or implement a functioning secret agent. Use NetworkManager itself for authentication.

## Demonstration

See `../tests/test_nm-secret-agent.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-nm-secret-agent.h`](../include/mulib-nm-secret-agent.h) |
| Implementation switch | `MULIB_IMPL_NM_SECRET_AGENT` |
| Repository-local includes | [mulib-dbus-client](mulib-dbus-client.md) |
| Complete example | [`examples/modules/nm-secret-agent.c`](../examples/modules/nm-secret-agent.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nm-secret-agent.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
