# Column table

Define `MULIB_IMPL_COLUMN_RDBMS`; no MuLIB dependencies. Zero-initialize and set
`columns`. Atomic append accepts finite doubles only. Column views remain valid
until mutation/free. Dimensions must not change while live. No SQL/nulls.
[Example](../examples/modules/column-rdbms.c), [contracts](storage-server.md).
