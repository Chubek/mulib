# mulib-search-rank: Deterministic lexical score for ranking title/description matches.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Deterministic lexical score for ranking title/description matches. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SEARCH_RANK` in exactly one C11 translation unit. Use `#include "mulib-search-rank.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-search-rank.c -o /tmp/test-search-rank
/tmp/test-search-rank
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/search-rank.c -o /tmp/example-search-rank
```

## API

```c
/* Overlapping ASCII case-insensitive substring count; capped at SIZE_MAX. */
size_t mulib_search_rank_count(const char *text,const char *query);
```

## Safety and ownership

`count` counts overlapping case-insensitive ASCII substring matches: `aaa` with
query `aa` yields 2. NULL text/query or an empty query yields zero. Non-ASCII
bytes compare literally. Allocation-free, worst-case O(text length × query
length); count saturates at SIZE_MAX.

`words` requires non-word boundaries on both sides; word bytes are ASCII
letters, digits and underscore. Query must start/end in a word byte but may
contain internal separators. `fields` sums whole-word title and description
counts with finite nonnegative weights. Invalid weights or score overflow
yield NaN. No Unicode folding or stemming occurs. Regression:
`tests/test_data_patches.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-search-rank.h`](../include/mulib-search-rank.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB search-rank: patched lexical counts, token-boundary and weighted fields.
 * Preserves mulib_search_rank_count byte-level ASCII semantics. */
#ifndef MULIB_SEARCH_RANK_H
#define MULIB_SEARCH_RANK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_search_rank_count (const char *text, const char *query);
  /* Count whole ASCII word matches without overlapping non-word characters. */
  size_t mulib_search_rank_words (const char *text, const char *query);
  /* Finite nonnegative weights; exact match counts may overlap. */
  double mulib_search_rank_fields (const char *title, const char *description,
                                   const char *query, double title_weight,
                                   double description_weight);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-search-rank.h`](../include/mulib-search-rank.h) |
| Implementation switch | `MULIB_IMPL_SEARCH_RANK` |
| Repository-local includes | None |
| Complete example | [`examples/modules/search-rank.c`](../examples/modules/search-rank.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/search-rank.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
