# Flat record schema

Define `MULIB_IMPL_DBSCHEMA`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`. Schemas and
field names are borrowed ASCII identifiers. Validation rejects duplicate fields;
record checking rejects unknown/missing-required/wrong-type members. Nullable
and required are independent. Nested values have no recursive constraints.
[Example](../examples/modules/dbschema.c), [contracts](storage-server.md).
