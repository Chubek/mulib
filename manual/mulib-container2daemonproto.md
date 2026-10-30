# container2daemonproto

Bounded transport-independent frame codec. Header: `MCDP`, version byte 1,
nonzero opcode byte, two zero reserved bytes, LE64 request ID, LE32 payload size;
then <=1 MiB payload and LE32 CRC32 of header+payload. Opcode is 1..255;
START/STOP/DELETE/STATUS are 1..4, IPC 16, response opcode=request|128.

`encode` appends atomically and permits input/output aliasing; 1 success, 0 error.
`decode`: 1 complete frame with borrowed payload and consumed count, 0 incomplete,
-1 invalid/version/CRC/limit. Extra stream bytes permitted; outputs unchanged
except on success. Request IDs are correlation tokens, not replay protection.
**CRC is not authentication**; caller supplies authenticated/authorized transport.
No sockets, encryption, peer verification, streaming reassembly or daemon.

Switches: `MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/container2daemonproto.c`.
