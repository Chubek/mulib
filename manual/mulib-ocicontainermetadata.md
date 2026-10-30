# ocicontainermetadata

`json` projects container identity/state to an owned shared JSON object with
ociVersion, id, status, exitCode. NULL means invalid/OOM; caller destroys result.
Deleted is a MuLIB extension. It deliberately does not fabricate PID or bundle
path, so this projection is **not a complete OCI state schema**. Lifecycle model
is borrowed during call; JSON copies identity. No filesystem persistence.

Switches: `MULIB_IMPL_OCICONTAINERMETADATA`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/ocicontainermetadata.c`.
