# containerserverd

Single-runtime request dispatcher, not a running daemon/server. Caller must
authenticate/authorize transport **before** `dispatch`. Requires exactly one
complete START/STOP/DELETE/STATUS frame whose payload matches runtime ID.
Replies echo request ID, opcode|128, payload JSON state. STOP invokes code-zero
backend action; actual supervision/exit observation belongs to backend.

Response allocation precedes backend action; failure restores response length
and does not publish lifecycle state. Backend must obey its failure-atomic
contract. Returns 0 success, -1 malformed/unsupported/mismatch, -2 allocation,
or nonzero backend error. No listener, process, thread, permissions or multi-ID
registry. Response and request may share storage.

Switches: `MULIB_IMPL_CONTAINERSERVERD`, `MULIB_IMPL_OCICONTAINERRUNTIME`,
`MULIB_IMPL_OCICONTAINERMETADATA`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/containerserverd.c`.
