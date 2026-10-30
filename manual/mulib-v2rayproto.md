# v2rayproto — VLESS v0 headers

Define `MULIB_IMPL_SOCKS5` and `MULIB_IMPL_V2RAYPROTO`.
See [`examples/modules/v2rayproto.c`](../examples/modules/v2rayproto.c).

This module implements VLESS version-zero request headers: version, 16-byte
UUID, zero addons length, command, big-endian port, address type and address.
TCP=1 and UDP=2 are supported. Address types are correctly translated between
VLESS (1 IPv4, 2 domain, 3 IPv6) and the shared SOCKS5 address model. Port zero
is rejected. A complete header's consumed length leaves any following payload
in caller storage; UUID/address data are copied into the result.

`decode` returns 1 complete, 0 incomplete, -1 malformed/invalid, -2 unsupported
version/addons/command. Outputs stay unchanged on failure and used is zero.
`encode` returns 1/0 with the same supported profile and resets used on failure.
All buffers are borrowed; output must not overlap the source. No allocation.

UUIDs identify clients; they do not encrypt or authenticate transport traffic
by themselves. Authorization and an authenticated transport belong to the
application. VMess, VLESS addons/flow extensions, multiplexing, response
headers and UDP payload framing are outside this request-header API.
Tests: `tests/test_protocol_extensions.c`, including every incomplete prefix.
