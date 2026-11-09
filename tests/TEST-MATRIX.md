# Test matrix and expected caveats

All 92 modules have link/symbol smoke tests. Core behavioral tests include: netlink socket lifecycle, netlink attributes and schema, link discovery, cap/ns reads, RSN parsing, ICMP checksum, PCAP format, DHCPv4/v6 message generation, IPv6 NDP/RA parsing, profile parsing, sysfs loopback, IPv4/IPv6 headers, TCP/UDP codec checksums, TCP FSM subset, packet buffer, ARP cache, LPM routing, fragment reassembly, DNS query, D-Bus basic transport errors and userspace clock.

The six cross-module networking suites cover malformed packet lengths,
Ethernet/ARP/ICMP/UDP datapaths, mock D-Bus replies, parser/cache/buffer
hardening, and deterministic Netlink transactions. Regressions exercise
unaligned byte buffers, echoed extended ACK payloads, interrupted dumps,
overruns, sequence wraparound, invalid error signs, ARP update/expiry,
fragment overlap/final-size conflicts, checksum carry overflow, DNS compression
cycles, RSN optional-field truncation, and D-Bus body/variant boundaries.
The include-order regression also checks kernel/libc header composition and
implementation switches enabled after declaration-only includes.

Privileged operations (address mutation, queue bind, nftables mutations, raw sockets) are intentionally **not executed** on host interfaces by default. To test them, use disposable network namespaces and a dedicated CI job with CAP_NET_ADMIN/CAP_NET_RAW. D-Bus integration should be tested against a fake, isolated D-Bus service and live system daemon only after protocol compliance review.

Avoid running the networking examples with `sudo`; the examples supplied are read-only or offline packet construction.
