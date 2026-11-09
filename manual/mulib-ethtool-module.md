# mulib-ethtool-module

**Retrieve transceiver SFP/module EEPROM type and size via real ETHTOOL_GMODULEINFO ioctl**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_ETHTOOL_MODULE` once before including `mulib-ethtool-module.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_ethtool_module_info(const char *interface,uint32_t *module_type,uint32_t *eeprom_bytes);
```

## Semantics and safety

Linux ETHTOOL_GMODULEINFO ioctl; returns module type/eeprom_len. Does not read EEPROM data (ETHTOOL_GMODULEEEPROM) or configure transceivers. Unsupported/nontransceiver interfaces return negative errno.

## Demonstration

See `../tests/test_ethtool-module.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ethtool-module.h`](../include/mulib-ethtool-module.h) |
| Implementation switch | `MULIB_IMPL_ETHTOOL_MODULE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ethtool-module.c`](../examples/modules/ethtool-module.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ethtool-module.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
