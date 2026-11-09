# mulib-net-profile

**Parse a bounded flat INI-style network profile with interface, IPv4 address and gateway fields**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_PROFILE` once before including `mulib-net-profile.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_net_profile {char interface[32],address[48],gateway[48];unsigned enabled;} mulib_net_profile_t;
int mulib_net_profile_parse(const char *text,size_t length,mulib_net_profile_t *out);
```

## Semantics and safety

The length-delimited input may not contain embedded NUL bytes. Address and
gateway values are copied as strings, not validated as IP literals or applied
to the host. The reconciliation helper also requires a terminated interface
array and an `enabled` value of 0 or 1 in caller-constructed profiles.

Flat key=value format, no sections, 4 recognized keys only, no whitespace trimming of values. Parser does not apply configuration. Rejects unknown properties.

## Demonstration

See `../tests/test_net-profile.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-profile.h`](../include/mulib-net-profile.h) |
| Implementation switch | `MULIB_IMPL_NET_PROFILE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/net-profile.c`](../examples/modules/net-profile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-profile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
