# AMD64

Define `MULIB_IMPL_AMD64` and the [assembler](mulib-assembler.md) dependency closure.
Zero-initialize the context, call `mulib_amd64_init`, emit typed instructions and
free with `mulib_amd64_free`. It owns generated dsymspec metadata; it is not a
second encoder or a text assembler. See [example](../examples/modules/amd64.c)
and [shared contracts](storage-server.md).
