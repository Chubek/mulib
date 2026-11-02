# IPv4 access control

Define `MULIB_IMPL_ACL`. Borrowed first-match CIDR rules; addresses have numeric
network significance (127.0.0.1 = 0x7f000001), independent of native byte order.
Prefix 0 matches all, 32 is exact. Invalid rules/default deny everything.
[Example](../examples/modules/acl.c), [contracts](storage-server.md).
