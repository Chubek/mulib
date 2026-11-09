# mulib-dns-manager

**Read first nameserver IP literal from resolv.conf without changing system DNS**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DNS_MANAGER` once before including `mulib-dns-manager.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_dns_manager_first_nameserver(const char *path,char *out,size_t cap);
```

## Semantics and safety

On Linux, a nameserver token must pass IPv4 or IPv6 literal validation; invalid
tokens return `-EBADMSG`. Zone-ID suffixes are not supported. Lines longer than
511 bytes return `-E2BIG` rather than being parsed as independent chunks. This
routine never writes the configuration file.

Simple resolv.conf first nameserver text reader. Does not resolve per-link systemd-resolved state, support DNS-over-TLS, or write any files.

## Demonstration

See `../tests/test_dns-manager.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dns-manager.h`](../include/mulib-dns-manager.h) |
| Implementation switch | `MULIB_IMPL_DNS_MANAGER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dns-manager.c`](../examples/modules/dns-manager.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dns-manager.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
