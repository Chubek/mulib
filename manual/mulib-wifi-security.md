# mulib-wifi-security

**Strict bounded WPA2 RSN information element parsing**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_WIFI_SECURITY` once before including `mulib-wifi-security.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_wifi_security_rsn {uint16_t version;uint16_t pairwise_count,akm_count;uint8_t group_suite[4];} mulib_wifi_security_rsn_t;
int mulib_wifi_security_rsn_parse(const void *ie,size_t len,mulib_wifi_security_rsn_t *out);
```

## Semantics and safety

Pairwise and AKM lists must be nonempty. After those lists the parser accepts
optional capabilities, a bounded PMKID list, and an optional four-byte group
management cipher suite, in that order. Partial optional fields and trailing
garbage are rejected. Parsing does not establish that the selected cipher or
authentication suite is secure or supported by the device.

Decodes RSN element ID=48, count/length/version/group cipher. Does not perform cryptography, certificate validation or Wi-Fi association. Array cipher details remain in caller packet.

## Demonstration

See `../tests/test_wifi-security.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wifi-security.h`](../include/mulib-wifi-security.h) |
| Implementation switch | `MULIB_IMPL_WIFI_SECURITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/wifi-security.c`](../examples/modules/wifi-security.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wifi-security.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
