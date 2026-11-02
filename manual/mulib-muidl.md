# MuIDL record generation

Define `MULIB_IMPL_MUIDL`, `DBSCHEMA`, `BUFFER` and the schema dependency closure.
Generates the small `record NAME { NAME: TYPE[?][ optional]; }` grammar defined
in the header, not a parser/compiler. Append is atomic, without a trailing NUL.
[Example](../examples/modules/muidl.c), [contracts](storage-server.md).
