# mulib-inte6 (IPv6)

The requested filename is **inte6**; the public symbols use `mulib_inet6_*`
and the implementation switch is **`MULIB_IMPL_INET6`**.

## Integration and supported syntax

Define `MULIB_IMPL_PARSEC`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6` before including
`mulib-inte6.h` in one C11 translation unit. No native networking library needed.
Hex groups are recognized with MuLIB combinators; embedded decimal addresses
reuse `mulib-inet4`. Addresses occupy sixteen copyable network-order bytes.

`parse(text,length,&address)` accepts exactly eight hex groups of 1..4 digits,
or fewer groups with exactly one `::` replacing at least one zero group. A final
strict dotted IPv4 address occupies two groups. Maximum input is 45 bytes.
Rejects brackets, zones, URI percent escapes, IPvFuture, ports, whitespace,
embedded NUL, multiple compressions and incorrect group counts.

## Formatting and prefixes

`format(&address,out,capacity)` emits lowercase hexadecimal, removes leading
zeroes and compresses the longest zero run of length >=2 (first run wins ties).
IPv4-mapped addresses are also emitted as hex; there is no dotted-output mode.
40 bytes always suffice, including the NUL terminator.

`network` and `contains` mirror IPv4, accepting prefixes 0..128. Operations
return 1 on success/membership, otherwise 0. Parsing/formatting/network outputs
remain unchanged on failure, including allocation failure inside IPv4 captures.
No output allocation or retained source pointers. Network output may alias input.

## Example and tests

`cc -std=c11 -Iinclude examples/modules/inte6.c -o inte6`.
`tests/test_network.c` checks canonicalization, malformed forms, every prefix,
and 2,000 roundtrips compared against native `inet_pton` on POSIX.
