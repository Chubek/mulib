# WIT record generation

Define `MULIB_IMPL_WIT`, `DBSCHEMA`, `BUFFER` and the schema dependency closure.
Only bool/f64/string fields and lowercase underscore-separated identifiers are
supported. Names become escaped kebab-case. Optional/nullable each add an option
layer. Append is atomic without a NUL. No parser/component runtime/bindings.
[Example](../examples/modules/wit.c), [mapping](storage-server.md#idl-and-native-output).
