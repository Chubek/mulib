# mulib-spellcheck

[Manuals](README.md) · [Library index](index.md)

Configurable spell recognition and suggestion algorithms, using existing MuLIB
dictionary, edit, trie, BK-tree and finite-state primitives. No system word
lists, Hunspell data, Unicode normalization or implicit case folding are loaded.

## Integration

Define `MULIB_IMPL_SPELLCHECK`, `MULIB_IMPL_CACHE`, `MULIB_IMPL_KVDB`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_CRC32`,
`MULIB_IMPL_VARINT`, `MULIB_IMPL_HATTRIE`, `MULIB_IMPL_HASH`, `MULIB_IMPL_TRIE`,
`MULIB_IMPL_UTF8`, `MULIB_IMPL_FST`, `MULIB_IMPL_FST_SERIALIZE`,
`MULIB_IMPL_VEC`, `MULIB_IMPL_BKTREE`, `MULIB_IMPL_BK_TREES`,
`MULIB_IMPL_LEVENSHTEIN`, `MULIB_IMPL_WEIGHTED_EDIT_DISTANCE` and
`MULIB_IMPL_WEIGHED_EDIT_DISTANE` in one C11 translation unit. Link libm.
The last spelling is an existing compatibility dependency, not a second distance
implementation. See the [standalone example](../examples/modules/spellcheck.c).

## Recognition

`create(cache_backend)` owns a selectable indexed dictionary and copied rules.
`add_word` stores a nonempty valid UTF-8 word (<=256 bytes) with uint64 frequency.
Exact matching is case-sensitive. `check` returns 1 correct, 0 incorrect, or a
negative INVALID/MEMORY/LIMIT status. Zero flags select dictionary lookup;
otherwise choose any of these recognition methods:

- **DICTIONARY:** exact selected-cache lookup.
- **AFFIX:** inverse literal prefix/suffix transformations. Register
  `add_affix(kind, strip, restore, minimum_stem_scalars)`; e.g. suffix `ies -> y`
  lets `cities` resolve to dictionary root `city`. At most one prefix and one
  suffix combine; final restored roots must be dictionary words. Up to 128
  rules, each strip/restore <=256 UTF-8 bytes, nonempty strip. This is explicit
  morphological analysis, not recursive stripping or automatic linguistic rules.
- **COMPOUND:** dynamic programming over scalar boundaries decomposes a word into
  2..max_parts recognized components. Components require the configured minimum
  scalar count. Combine AFFIX to recognize inflected components. No arbitrary
  substring break inside UTF-8 is permitted.
- **FST:** a copied finite-state machine recognizes the entire word, consuming
  UTF-8 **bytes as uint32 labels**; outputs are ignored. Missing transitions or
  nonaccepting final states reject. This byte convention is deliberately distinct
  from hyphenation's scalar-label FST interface.

## Suggestions

`suggest` supports these independent algorithms:

1. **LEVENSHTEIN:** MuLIB's existing two-row byte Levenshtein distance over all
   dictionary candidates.
2. **WEIGHTED:** directional byte insertion/deletion/substitution costs with
   weighted DP. Zero costs and non-integral thresholds are legal.
3. **TRIE:** build a dictionary trie and carry edit-DP rows along edges, pruning
   a subtree when its row minimum exceeds the unit-distance radius.
4. **BKTREE:** build a BK metric index, then use triangle-inequality pruning.
5. **TRANSDUCER:** walk accepting FST byte paths with edit-DP rows, expanding
   clipped byte ranges 1..255. Cycles are bounded by word length 256 and shared
   work budget. No external dictionary entry is required for an accepted path;
   dictionary frequency is used if available, otherwise zero.

Trie/BK indexes are built per suggestion call, so add_word cannot leave stale
indexes after mutation. Index-building work is part of the query budget.
Unit engines use `floor(max_distance)` for pruning; they never apply nonmetric
weighted costs to a metric tree. FST outputs are not interpreted as suggested
words: accepted input paths are the spelling lexicon.

Results are owned copies sorted by distance ascending, frequency descending,
then byte-lexicographic spelling. Duplicates are eliminated. A bounded top-k
list avoids storing all matches. Output must be zero-initialized or a previous
result; only success replaces it. `result_free` releases all words and storage.

`default_options()` selects distance 2, edit costs 1, 16 results, 10000000 work
units, at most 8 compound components and minimum 2 scalars/component. When an
options struct is supplied, zero numeric distances/costs are meaningful; zero
size limits select defaults. Max results 4096, max components/minimum 256.
Budgets count DP cells, graph/trie visits and morphology trials, not wall time.
Negative errors discard partial owned suggestions; standalone tree callbacks
have their separate partial-delivery contracts.

UTF-8 is validated but distances intentionally count **bytes**, preserving the
existing Levenshtein metric. A diacritic may cost multiple edits. Use the token
weighted-edit API for scalar/grapheme-aware application-specific distances.
There are no phonetic models, context-sensitive ranking, transpositions,
automatic compound linking morphemes, or full Hunspell flag/condition semantics.

## Persistence

`dictionary_load` atomically imports one UTF-8 word per LF/CRLF line at frequency
1 (no comments). `serialize`/`deserialize` roundtrip the entire versioned model,
including dictionary frequencies, affix transforms and FST graph/start. Snapshot
records are `D`+word -> LE64 frequency, `A`+LE32 ordinal -> structured affix,
and reserved `@format`/`@fst` metadata. CRC-protected cache decoding validates all
records and rebuilds the selected index. Pointers are never serialized.

Text imports cap at 16 MiB, model decoding at 64 MiB. Callers explicitly store
serialized models/results in `mulib-cache` and choose disk paths, namespace,
model versions and invalidation policy. `destroy` never commits files.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spellcheck.h`](../include/mulib-spellcheck.h) |
| Implementation switch | `MULIB_IMPL_SPELLCHECK` |
| Repository-local includes | [mulib-bktree](mulib-bktree.md), [mulib-cache](mulib-cache.md), [mulib-endian](mulib-endian.md), [mulib-fst](mulib-fst.md), [mulib-utf8](mulib-utf8.md), [mulib-weighted-edit-distance](mulib-weighted-edit-distance.md) |
| Complete example | [`examples/modules/spellcheck.c`](../examples/modules/spellcheck.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spellcheck.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
