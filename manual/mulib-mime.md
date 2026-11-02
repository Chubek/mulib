# MIME lookup

Define `MULIB_IMPL_MIME` and `SECURITY`. Case-insensitive filename extensions
select static MIME strings. Unknown/missing extensions return
application/octet-stream. No content sniffing or allocation.
[Example](../examples/modules/mime.c), [contracts](storage-server.md).
