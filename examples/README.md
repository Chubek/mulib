# MuLIB networking examples

Run `./examples/build.sh`, then execute programs under `examples/build/`.
Each of the 92 networking modules also has a checked standalone example under
`examples/modules/`; use `cmake -S . -B build -DMULIB_BUILD_MODULE_EXAMPLES=ON`
to include these in the normal CMake build and CTest suite. Hardware/daemon
queries report unavailable features rather than requiring those services.

- `ip-links`, `show-route`: read live kernel link and route dumps via rtnetlink.
- `generic-netlink`: resolve the nl80211 family name, if present.
- `socket-diagnostics`: count IPv4 TCP sockets from SOCK_DIAG.
- `nm-state`: query NetworkManager if D-Bus and service are present.
- `icmp-echo`, `udp-packet`, `dhcp4-discover`, `dns-query`: build/validate protocol packets without sending them.
- `ustack-demo`: offline ARP request → Ethernet ARP reply demonstration through actual userspace stack.
- `network-profile`: read-only loopback state comparison.
- `pcap-write`: emit a PCAP global header to `/tmp/mulib-demo.pcap`.

These examples require no root privileges. No routing/interface mutation is performed.
