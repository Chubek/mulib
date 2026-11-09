# mulib-wifi-rfkill

**Read hardware and software radio block state from /dev/rfkill**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_WIFI_RFKILL` once before including `mulib-wifi-rfkill.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_wifi_rfkill_state {uint32_t index;uint8_t type,operation,soft,hard;} mulib_wifi_rfkill_state_t;
int mulib_wifi_rfkill_read(int fd,mulib_wifi_rfkill_state_t *out);
int mulib_wifi_rfkill_open(void);
```

## Semantics and safety

Read-only. Kernel rfkill event may be read repeatedly for state. Open fd caller-owned; close on completion.

## Demonstration

See `../tests/test_wifi-rfkill.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wifi-rfkill.h`](../include/mulib-wifi-rfkill.h) |
| Implementation switch | `MULIB_IMPL_WIFI_RFKILL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/wifi-rfkill.c`](../examples/modules/wifi-rfkill.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wifi-rfkill.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
