# mulib-netpolicy: URL scheme and obvious loopback/private-host denial policy.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

URL scheme and obvious loopback/private-host denial policy. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_NETPOLICY` and `MULIB_IMPL_URLCANON` in exactly one C11 translation unit. Include `mulib-netpolicy.h`; implementations are never enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-netpolicy.c -o /tmp/test-netpolicy
/tmp/test-netpolicy
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/netpolicy.c -o /tmp/example-netpolicy
```

## API

```c
/* Deny local hostnames, raw private/loopback IPv4 ranges, and non-HTTP(S) schemes. Not a DNS-resolution SSRF defense. */
int mulib_netpolicy_public_url(const char *url);
```

## Safety and ownership

Returns 1 only for conservatively accepted HTTP(S) DNS names or canonical dotted IPv4; returns 0 otherwise. URL syntax and the 8192-byte input limit follow [urlcanon](mulib-urlcanon.md). Single-label/local suffix names, credentials, IPv6 (including public IPv6), alternate numeric IPv4 forms, leading-zero octets, invalid ports, private/loopback/link-local/shared/multicast and selected reserved/documentation IPv4 ranges are rejected. ASCII case and a trailing DNS dot do not bypass local-name checks. No allocation occurs.

**This is not an SSRF defense.** DNS names may resolve to private addresses or change between checks. Resolve and validate every address at connection time, bind the checked address to the connection, and re-check every redirect. Proxy behavior, DNS rebinding, special-use domain registries and all non-global address registries are outside this lexical helper's scope.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netpolicy.h`](../include/mulib-netpolicy.h) |
| Implementation switch | `MULIB_IMPL_NETPOLICY` |
| Repository-local includes | [mulib-urlcanon](mulib-urlcanon.md) |
| Complete example | [`examples/modules/netpolicy.c`](../examples/modules/netpolicy.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netpolicy.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
