# Token-frequency corpus

Define `MULIB_IMPL_CORPUS` and `MULIB_IMPL_TRIE`.
[Example](../examples/modules/corpus.c).

Zero-initialize or init uninitialized storage. Add copies binary tokens and
adds a positive uint64 count; token and total overflow, invalid input and OOM
fail without changing counts. Empty tokens work through add. Count returns
zero for absent/invalid queries. `tokens.size` is vocabulary size; `total` is
token occurrences. Free resets all owned data; no shallow copying live objects.

Ingest accepts a C string, extracts ASCII letter runs and folds A-Z to a-z.
Digits, punctuation, whitespace and non-ASCII bytes separate runs. It first
copies the input, then adds incrementally: failure preserves already-added
tokens, not an all-or-nothing transaction. Count/add do not implicitly fold.
No stemming, Unicode segmentation, document-frequency or file-loader API.
Original fixtures live in `library/statistical-text`.
