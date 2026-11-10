# stopwords — borrowed sorted vocabulary

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_STOPWORDS`.

Configure a borrowed array of non-NULL NUL-terminated words sorted by `strcmp`.
Contains returns 1 for exact membership, 0 for absent/invalid set or query.
There is no built-in vocabulary, case folding, sorting or ownership transfer.
Normalize words with the same rules as the tokenizer before constructing the
set. The array and strings must outlive all lookups/analyzers borrowing them.
Binary search costs O(log count) string comparisons and allocates nothing.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-stopwords.h`](../include/mulib-stopwords.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Sorted static stopword set lookup for normalized tokens. C11 single-header;
 * define MULIB_IMPL_STOPWORDS once. */
#ifndef MULIB_STOPWORDS_H
#define MULIB_STOPWORDS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *const *words;
    size_t count;
  } mulib_stopwords_t;
  /* words must be sorted with strcmp; returns 1 if present. */
  int mulib_stopwords_contains (const mulib_stopwords_t *set, const char *word);
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
| Header | [`include/mulib-stopwords.h`](../include/mulib-stopwords.h) |
| Implementation switch | `MULIB_IMPL_STOPWORDS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/stopwords.c`](../examples/modules/stopwords.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/stopwords.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
