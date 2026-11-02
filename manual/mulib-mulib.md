# Core umbrella

Define `MULIB_IMPL_MULIB` for `mulib_version()`. The header exposes buffer/strbuf
declarations and compile-time version constants, not every optional module.
Define `BUFFER`/`STRBUF` implementations separately if using their functions.
[Example](../examples/modules/mulib.c), [contracts](storage-server.md).
