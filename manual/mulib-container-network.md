# mulib-container-network

## NAME AND INTEGRATION

IPv4/IPv6 longest-prefix routing and ordered policy over virtual endpoints.
Define `MULIB_IMPL_CONTAINER_NETWORK`, `MULIB_IMPL_CONTAINER_NETDEV`,
`MULIB_IMPL_PACKETRULES`, `MULIB_IMPL_PACKETFITER`. Example:
`examples/modules/container-network.c`.

## ROUTES

`init(network,max_routes,max_rules,default_action)` creates an empty route/policy
table. max_routes is positive; rule limits are at most 65536. Default action is
exactly 0 drop or 1 accept. `add` copies a route and canonicalizes host bits to
zero. IPv4 prefixes are 0–32; IPv6 0–128. The endpoint is borrowed and must outlive
all routes referring to it.

Routes contain family, network bytes, prefix, metric, endpoint and explicit
next-hop MAC. Same family/network/prefix/metric is a duplicate. `lookup` ignores
down endpoints and chooses longest prefix then lowest metric. Returned pointers
are borrowed until route mutation. `remove(index)` compacts the route table.

## POLICY AND DELIVERY

`policy` validates/copies all rules before replacing the existing policy.
`check` parses one L3 packet and applies first-match `packetrules` policy, returning
OK, EACCESS or EFORMAT; optional matched index follows packet-rules semantics.
accepted/denied counters count policy decisions, not guaranteed wire delivery.

`send` checks policy, parses destination, finds a route and enforces MTU. Raw-IP
endpoints receive the original L3 bytes. Ethernet endpoints receive a newly
constructed 14-byte header using device source MAC, route next-hop MAC and IPv4/
IPv6 EtherType. No ARP/NDP is attempted. No route produces ENOENT and increments
unroutable. Endpoint AGAIN/error propagates without retry.

## SUPPORTED PACKETS

Parsing follows `packetfiter`: IPv4 options are skipped, fragments have no exposed
ports, and unsupported IPv6 extension chains fail. Checksums are not verified.
Trailing link padding is permitted by the parser and preserved by send; MTU applies
to the supplied packet length. This is endpoint routing, not an IP forwarder:
there is no TTL decrement, fragmentation, NAT or host-network modification.

`free` releases routes/rules, never endpoints. Mutation during transmit callback
is rejected with EBUSY; callers externally serialize all access. Tests cover
Ethernet wrapping, receive pressure, route matching and default-drop policy.
