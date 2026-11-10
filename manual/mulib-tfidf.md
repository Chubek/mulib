# tfidf — smoothed term scoring

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_TFIDF`; link `-lm`.

IDF is `log((N+1)/(df+1)) + 1`; score multiplies it by tf, or by `1+log(tf)`
when sublinear is nonzero. tf=0 scores zero after validating N/df. Inputs must
be finite, N ≥ 0, 0 ≤ df ≤ N and tf ≥ 0; invalid or unrepresentable scores
return NaN. Fractional positive tf is allowed; sublinear weighting can therefore
be negative below exp(-1). No vector normalization is applied. Constant-time,
allocation-free; aggregate per-term scores in the caller.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tfidf.h`](../include/mulib-tfidf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: TF-IDF classic and sublinear formulas with log-smoothed IDF. C11
 * single-header; define MULIB_IMPL_TFIDF once. */
#ifndef MULIB_TFIDF_H
#define MULIB_TFIDF_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  double mulib_tfidf_idf (double documents, double document_frequency);
  double mulib_tfidf_score (double term_frequency, double documents,
                            double document_frequency, int sublinear);
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
| Header | [`include/mulib-tfidf.h`](../include/mulib-tfidf.h) |
| Implementation switch | `MULIB_IMPL_TFIDF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/tfidf.c`](../examples/modules/tfidf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tfidf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
