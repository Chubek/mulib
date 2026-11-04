# Byte n-grams

Define `MULIB_IMPL_N_GRAMS`. [Example](../examples/modules/n-grams.c).

`mulib_ngrams` enumerates every overlapping width-byte window and its byte
offset. Windows borrow the source, are not NUL-terminated, and are not copied.
Callback nonzero continues; zero stops. Result counts callbacks including the
stopping call. Zero width, missing callback or NULL nonempty source returns
SIZE_MAX. Width greater than length yields zero calls. NULL empty source works
with positive width. No allocation; O(length) calls, constant storage.
Tokenization, frequency counting, boundary padding and Unicode segmentation
are deliberately separate concerns.
