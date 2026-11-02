# WebIDL dictionary generation

Define `MULIB_IMPL_WEBIDL`, `DBSCHEMA`, `BUFFER` and the schema dependency closure.
Generates dictionaries, not parsers or bindings. Names use underscore escaping;
null-only fields fail, arrays map to sequence<any>. Append is atomic, without a
trailing NUL. [Example](../examples/modules/webidl.c),
[mapping and limits](storage-server.md#idl-and-native-output).
