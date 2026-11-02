# URL rewriting

Define `MULIB_IMPL_REWRITE_URL` and `SECURITY`. Replaces a strict origin-path
component prefix, preserving raw query bytes. No regex or redirect construction.
Returns 1 with malloc-owned output, 0 no match, -1 invalid/OOM. Output is
unchanged unless successful. [Example](../examples/modules/rewrite_url.c),
[contracts](storage-server.md#http-and-policies).
