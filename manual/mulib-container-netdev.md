# mulib-container-netdev

## NAME AND INTEGRATION

Virtual packet endpoints with bounded receive queues and whole-packet transports.
Define `MULIB_IMPL_CONTAINER_NETDEV`; inline container core is its dependency.
Example: `examples/modules/container-netdev.c`.

## ENDPOINTS

`init(device,name,kind,mtu,packets,queued_bytes)` allocates a receive ring. Kind
is IP (raw L3) or ETHERNET (14-byte header). MTU is 68–65535 L3 bytes; Ethernet
permits MTU+14 total. packets must be positive. Devices start link-down, with a
default locally administered unicast MAC. Packet payload must be nonempty.

`attach(device,transmit,user,mac)` installs a borrowed whole-packet callback and
optional nonzero unicast MAC. NULL transmit detaches. Callback returns OK for a
fully accepted packet, AGAIN for retry, or a negative CT error; no partial packet
acceptance exists. Context remains application-owned.

`link(device,up)` changes link state without dropping queued data. Down links
reject inject/send/receive. `peer` is a ready-made transmit callback that injects
into another borrowed endpoint; it supports explicit in-memory links and loopback.

## RECEIVE AND SEND

`inject` copies inbound packets, bounded by both ring slots and byte budget. Full
queues return AGAIN and increment dropped without consuming input. `receive`
copies/removes the oldest packet. Empty queue returns AGAIN. Too-small output
returns ELIMIT, reports required length and retains the packet. Successful packet
counts/byte counters saturate; rx records accepted injection, tx records successful
callback delivery.

`send` checks link/MTU and invokes transmit once. Callback reentrant sends, attach,
link changes and receives are rejected with EBUSY. Injection into a sending
endpoint is allowed so loopback can enqueue. Same-object cross-thread access still
requires external synchronization. A busy free is ignored; release after callbacks.

## SCOPE AND LIFETIME

This layer preserves packet boundaries and backpressure; it does not validate IP
checksums, implement sockets, ARP/NDP or configure native interfaces. Connect a
TUN/TAP or other transport through the callback, with its own lifetime/error policy.
`free` releases queued bytes/table, never the transport context or peer endpoint.
Use [container-network](mulib-container-network.md) for routing/filtering.
