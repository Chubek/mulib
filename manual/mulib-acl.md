# IPv4/IPv6 access control

Define `MULIB_IMPL_ACL`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INET4` and
`MULIB_IMPL_PARSEC` in one implementation translation unit.

Rules are borrowed, ordered and first-match, **not** longest-prefix-match.
IPv4 addresses have numeric network significance (`127.0.0.1 = 0x7f000001`),
independent of host byte order. IPv6 uses `mulib_inet6_t` network-order bytes.
Prefix 0 matches everything; 32/128 is an exact address. Empty policies use the
explicit default (0 or 1). NULL rules are allowed only for an empty policy.

`mulib_acl_rule_parse` / `mulib_acl6_rule_parse` parse an explicit `address/prefix`
and normalize host bits. Reject missing prefixes, whitespace, signs, leading-zero
prefix lengths, invalid addresses, IPv6 zones/brackets, and out-of-range prefixes.
Parser outputs are unchanged on failure. IPv4 address octets reject leading zeros.
Manually supplied rules may retain host bits; membership ignores them.

`mulib_acl_validate` / `mulib_acl6_validate` inspect the **whole** array before any
rule is allowed to authorize. A malformed unreachable rule also denies the whole
policy. `mulib_acl_check_ex` and `mulib_acl6_check` optionally report the first
matched index, or `SIZE_MAX` for fallback/invalid input. Invalid defaults deny.
The original `mulib_acl_check` remains available. Checks do not allocate; parsers
use the existing address-parser dependencies and fail on allocation failure.

Use the actual transport peer address. Never trust client-supplied forwarding
headers without a separately authenticated trusted-proxy policy. IPv4-mapped IPv6
addresses do not automatically enter the IPv4 ruleset. Address ACLs are not user
authentication, and concurrent mutation of a borrowed rule array is unsupported.

[Example](../examples/modules/acl.c), [tests](../tests/test_authentication.c).
