# ociimage

Focused OCI image-manifest JSON profile. `digest` requires `sha256:` plus exactly
64 lowercase hex digits. `descriptor` requires nonempty mediaType, that digest
and integral size 0..2^53-1. `validate` requires schemaVersion 2, OCI manifest
mediaType, config descriptor and <=4096 layer descriptors. Unknown extension
members remain in the shared JSON tree. Validators return 1 valid, 0 invalid.

`parse` returns an owned validated JSON tree or NULL with optional diagnostic;
destroy with `mulib_json_destroy`. No downloads, hash/content-size verification,
signatures, platform config checks, indexes, Docker media-type compatibility or
layer decompression. Syntax validation does **not authenticate image content**.
Switches: `MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`. Example: `examples/modules/ociimage.c`.
