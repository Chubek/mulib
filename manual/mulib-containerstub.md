# containerstub

Client request encoder for START/STOP/DELETE/STATUS. `request` validates container
ID through the shared identity model, then appends a frame whose payload is ID
bytes without NUL. Request ID is echoed by server replies. Returns 1 success,
0 invalid/OOM without partial append. STOP requests backend's code-zero action.

No connection, authentication, retry, RPC correlation store or process creation.
Caller supplies transport/security and decodes replies with the shared protocol.
Switches: `MULIB_IMPL_CONTAINERSTUB`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`. Example: `examples/modules/containerstub.c`.
