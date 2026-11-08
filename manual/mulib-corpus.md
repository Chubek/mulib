# Token-frequency corpus

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-corpus.h`](../include/mulib-corpus.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-corpus.h
 * @brief Owned token-frequency corpus backed by the MuLIB byte trie.
 * Define MULIB_IMPL_CORPUS and MULIB_IMPL_TRIE.
 */
#ifndef MULIB_CORPUS_H
#define MULIB_CORPUS_H
#include "mulib-trie.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_corpus { mulib_trie_t tokens; uint64_t total; } mulib_corpus_t;
/** Initialize an uninitialized corpus; zero initialization is also valid. */
void mulib_corpus_init(mulib_corpus_t *corpus);
/** Add count occurrences of a binary token; count must be positive.
 * Return 0 on invalid input, overflow or OOM without changing counts. */
int mulib_corpus_add(mulib_corpus_t *corpus, const void *token, size_t length, uint64_t count);
/** Tokenize ASCII letter runs, fold to lowercase and add one per token.
 * Return 0 on failure; already-added tokens remain (incremental operation). */
int mulib_corpus_ingest(mulib_corpus_t *corpus, const char *text);
/** Frequency or zero when absent/invalid. No automatic folding. */
uint64_t mulib_corpus_count(const mulib_corpus_t *corpus, const void *token, size_t length);
/** Release all tokens and reset. */
void mulib_corpus_free(mulib_corpus_t *corpus);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-corpus.h`](../include/mulib-corpus.h) |
| Implementation switch | `MULIB_IMPL_CORPUS` |
| Repository-local includes | [mulib-trie](mulib-trie.md) |
| Complete example | [`examples/modules/corpus.c`](../examples/modules/corpus.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/corpus.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
