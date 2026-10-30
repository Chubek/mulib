# packetrules — ordered packet policy

Define `MULIB_IMPL_PACKETFITER` and `MULIB_IMPL_PACKETRULES`.
See [`examples/modules/packetrules.c`](../examples/modules/packetrules.c).

A rule selects IP family 4/6, protocol (zero wildcard), source/destination
prefixes, and optionally inclusive source/destination port ranges. Prefixes
are 0..32 or 0..128; network host bits are ignored. Rule action is 0 drop or
1 accept. A zero-initialized port range is port zero only when `ports=1`;
use 0..65535 for an unrestricted port range. Set `ports=0` to ignore ports.

`validate` checks all rules, including those that would be shadowed, with a
65536-entry maximum. `apply` validates the complete table, parses the packet,
and returns the first matching rule's action or the supplied default (0/1).
Port rules cannot match fragments or packets without decoded TCP/UDP ports.
Return -1 for invalid policy and -2 for malformed/unsupported packet; callers
should handle errors explicitly rather than interpret them as acceptance.
`matched` is the selected index or SIZE_MAX on default/error; it is optional.

No rule ownership, allocation, counters, mutable connection state, NAT or kernel
installation is supplied. Calls borrow the entire table and packet. The parser's
IPv6 extension-header limitations apply. Tests:
`tests/test_protocol_extensions.c` includes fragmented-packet bypass checks.
