# Architecture

```
kernel                     userspace frontends
NETLINK_ROUTE <-- mulib-netlink/rtnl <-- ip-like tools
NETLINK_GENERIC <-- mulib-netlink-genl <-- nl80211 / ethtool / devlink
NETLINK_SOCK_DIAG <-- mulib-sock-diag <-- ss-like tools
NETLINK_NETFILTER <-- mulib-nfnetlink <-- nft/conntrack read-only inspection
UNIX D-Bus socket <-- dbus-auth/transport/client <-- NetworkManager/systemd/iwd adapters
TAP Ethernet <-- mulib-ustack-tuntap <-- mulib-ustack
                                          | ARP cache, IPv4 parsing, ICMP echo, UDP handler
                                          | packet checksum/TCP codec/reassembly helpers
```

The userspace TCP segment codec **does not** add TCP functionality to `mulib-ustack`. The name 'client' in some DHCP/D-Bus modules means a bounded client-side building block, not a production-complete implementation. Protocol-specific manuals give precise limits.
