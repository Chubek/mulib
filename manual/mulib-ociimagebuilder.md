# ociimagebuilder

`descriptor` creates owned JSON from mediaType, externally computed sha256 digest
and size <=2^53-1. `create` validates/copies config into a manifest with empty
layers. `layer` validates/copies a descriptor into an existing valid manifest,
up to 4096 layers. Inputs remain caller-owned. Constructors return NULL on
invalid/OOM; layer returns 1 success, 0 error without changing manifest.

Use shared JSON stringify/destroy APIs. No blob hashing, file reads, compression,
registry upload, authentication or signature generation.
Switches: `MULIB_IMPL_OCIIMAGEBUILDER`, `MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/ociimagebuilder.c`.
