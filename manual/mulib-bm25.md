# bm25 — document and field ranking

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_BM25`; link `-lm`.

IDF is `log(1 + (N - df + 0.5)/(df + 0.5))`. BM25 multiplies it by
`tf*(k1+1)/(tf + k1*(1-b+b*length/average))`. BM25F first sums each field's
`weight*tf/(1-b+b*length/average)` and applies the same saturation.

All inputs must be finite: N > 0, 0 ≤ df ≤ N, tf/length/weight ≥ 0,
average > 0, k1 > 0 and 0 ≤ b ≤ 1. Field arrays must contain `fields` elements;
zero fields and zero field normalization are invalid. Invalid inputs or
unrepresentable results yield NaN. Valid zero frequency scores zero. Extended
intermediates reduce overflow where the platform's `long double` is wider.
Time is O(fields), with no allocation. Scores are per term; callers sum terms.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bm25.h`](../include/mulib-bm25.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Okapi BM25 and BM25F field scoring; finite checked inputs. C11 single-header;
 * define MULIB_IMPL_BM25 once. */
#ifndef MULIB_BM25_H
#define MULIB_BM25_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  double mulib_bm25_idf (double total_documents, double document_frequency);
  double mulib_bm25_score (double tf, double df, double doclen, double avglen,
                           double total, double k1, double b);
  double mulib_bm25f_score (const double *tf, const double *len, const double *avglen,
                            const double *weight, const double *b, size_t fields,
                            double df, double total, double k1);
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
| Header | [`include/mulib-bm25.h`](../include/mulib-bm25.h) |
| Implementation switch | `MULIB_IMPL_BM25` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bm25.c`](../examples/modules/bm25.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bm25.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
