# mulib-ethtool-features

**Read an actual 32-bit block of kernel ethtool available/requested/active feature bits**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_ETHTOOL_FEATURES` once before including `mulib-ethtool-features.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_ethtool_features_block {uint32_t available,requested,active,never_changed;} mulib_ethtool_features_block_t;
int mulib_ethtool_features_get(const char *interface,unsigned block_index,mulib_ethtool_features_block_t *out);
```

## Semantics and safety

Linux SIOCETHTOOL ETHTOOL_GFEATURES. Returns one actual block (32 feature bits) of available/requested/active/never_changed. Kernel may reject unsupported devices; no mutation. Caller selects block 0..31. This is the ioctl fallback, not Generic Netlink feature-bit name resolution.

## Demonstration

See `../tests/test_ethtool-features.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ethtool-features.h`](../include/mulib-ethtool-features.h) |
| Implementation switch | `MULIB_IMPL_ETHTOOL_FEATURES` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ethtool-features.c`](../examples/modules/ethtool-features.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ethtool-features.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
