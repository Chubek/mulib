# mulib-net-cli-format

**Format IPv4 and JSON-escaped interface names without third-party libraries**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_CLI_FORMAT` once before including `mulib-net-cli-format.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_net_cli_format_ipv4(const uint8_t ip[4],char *out,size_t cap);
int mulib_net_cli_format_json(const char *input,char *out,size_t cap);
```

## Semantics and safety

Only IPv4 dotted-quad and escaped UTF-8-as-bytes JSON strings; no full JSON document serializer. Returns bytes written or negative errno.

## Demonstration

See `../tests/test_net-cli-format.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-cli-format.h`](../include/mulib-net-cli-format.h) |
| Implementation switch | `MULIB_IMPL_NET_CLI_FORMAT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/net-cli-format.c`](../examples/modules/net-cli-format.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-cli-format.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
