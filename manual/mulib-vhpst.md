# Virtual hosts

The ambiguous historical filename is interpreted as a virtual-host helper.
Define `MULIB_IMPL_VHPST` and `SECURITY` (`SEURITY` also works). Lookup validates
an ASCII DNS hostname and performs case-insensitive exact matching. The table
and contexts are borrowed. No wildcard/IDNA/port/IP-literal handling; SIZE_MAX
means no match or invalid. [Example](../examples/modules/vhpst.c),
[contracts](storage-server.md#http-and-policies).
