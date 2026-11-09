# mulib-wpa-ctrl

**Send a single wpa_supplicant control request over an AF_UNIX datagram socket**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_WPA_CTRL` once before including `mulib-wpa-ctrl.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_wpa_ctrl_request(const char *interface,const char *command,char *response,size_t cap,size_t *got);
```

## Semantics and safety

Interface names must be nonempty, at most 15 characters, and contain only ASCII
letters, digits, `_`, `-`, or `.`. Commands must be nonempty and at most 4096
bytes. The datagram socket is connected to the selected control endpoint so
unrelated peers cannot supply the response. Receive and send timeouts are
installed and checked. Output is a byte span of `got` bytes, not necessarily a
NUL-terminated string. Commands are caller-selected and may change Wi-Fi state.

Linux wpa_supplicant standard local control directory /run/wpa_supplicant. Abstract local socket avoids creating temporary files. Requires service and permissions; no raw credentials logging. Return -EMSGSIZE on truncated responses.

## Demonstration

See `../tests/test_wpa-ctrl.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wpa-ctrl.h`](../include/mulib-wpa-ctrl.h) |
| Implementation switch | `MULIB_IMPL_WPA_CTRL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/wpa-ctrl.c`](../examples/modules/wpa-ctrl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wpa-ctrl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
