# netdevice — interface address snapshots

Define `MULIB_IMPL_NETDEVICE`; no dependencies.
See [`examples/modules/netdevice.c`](../examples/modules/netdevice.c).

On POSIX systems, `list` enumerates `getifaddrs` records. Each callback receives
the native interface name, index, flags, family, address and netmask. Family is
4 for IPv4, 6 for IPv6, or 0 for other records; non-IP address arrays are zero.
IPv6 scope ID is preserved. Multiple records may describe one interface and
interfaces may change during enumeration. Callback names/records are borrowed
only for the call. Return nonzero to stop; native snapshot storage is released
on completion or cancellation.

Results: 0 complete, 1 callback stopped, -1 invalid, -2 OS failure, -3
unsupported. `index(name)` resolves a native name to its index, or zero on
unknown/invalid/unsupported. No ownership is transferred. Flags are native
platform bits rather than a portable enum.

This is read-only discovery: address configuration, route changes, MTU changes
and link activation are external operations. Tests:
`tests/test_ipc_extensions.c` checks index consistency and callback cancellation.
