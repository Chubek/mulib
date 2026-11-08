# mulib-hyphen

[Manuals](README.md) · [Library index](index.md)

Six configurable hyphenation engines sharing UTF-8 boundary conventions and
portable model persistence. All linguistic data is supplied by the application;
there is no claim of universal language coverage or automatic language detection.

## Integration and ownership

Define `MULIB_IMPL_HYPHEN`, `MULIB_IMPL_ORHOCONV`, `MULIB_IMPL_DFA`,
`MULIB_IMPL_FST`, `MULIB_IMPL_FST_SERIALIZE`, `MULIB_IMPL_VEC`,
`MULIB_IMPL_CACHE`, `MULIB_IMPL_KVDB`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_ENDIAN`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_VARINT`, `MULIB_IMPL_HATTRIE`, `MULIB_IMPL_HASH`,
`MULIB_IMPL_TRIE`, and `MULIB_IMPL_UTF8` before including the header in one C11
translation unit. No dependency switches are enabled implicitly.

`create(cache_backend)` owns its model and conventions. Dictionary keys are
always indexed by HAT-trie; Liang patterns and morphemes by byte tries. The
selected cache backend controls the portable model's additional lookup index.
`conventions` borrows the owned configurable rule set (default minima 2,2);
`destroy` frees everything. Borrowed convention pointers are invalidated by
destruction or successful whole-dictionary loading, which swaps complete models.

`hyphenate(model, word, algorithm, &result)` replaces a zero-initialized/previous
owned result only on success. Free it with `result_free`. Results are ordered
**byte offsets** into the original UTF-8 word, never character-count offsets.
Every algorithm applies the shared orthographic filter. Empty words succeed
with no breaks. No normalization, case folding or grapheme normalization occurs.

## Engines

| Algorithm | Model and behavior |
| --- | --- |
| `DICTIONARY` | `add_word` copies exact UTF-8 words and strictly increasing internal byte splits into a HAT-trie-indexed dictionary. A known word may explicitly have no splits. Missing words return NOMATCH. |
| `ORTHOGRAPHIC` | Vowel nuclei and maximal registered consonant onsets propose syllable boundaries; adjacent vowel nuclei also propose hiatus boundaries. Registered literal prefix/suffix edges add boundaries. Rules/vowels/groups are stored in the convention KVDB. Forbidden splits and minima veto candidates. |
| `LIANG` | Original TeX/Liang maximum-pattern-weight overlay: pad with '.', match every pattern, take pointwise maxima, keep odd weights, then enforce minima. `hy1phen` proposes `hy-phen`; overlapping even weights can inhibit it. Duplicate patterns merge maxima, including Unicode-safe byte-boundary weights. |
| `MORPHOLOGY` | Serializable trie of literal morphemes with ROOT/PREFIX/SUFFIX flags. Dynamic programming recognizes prefixes* roots+ suffixes*, choosing the fewest components and proposing internal component edges. Unknown complete analyses return NOMATCH. |
| `FINITE_STATE` | Copied `mulib-fst` machine consumes Unicode scalars. Output 1 marks a break after the consumed symbol; output 0/no output does not. Full word must end in an accepting state. An optional copied DFA accepts prefixes at permitted split points, filtering FST proposals; a DFA alone proposes its accepting prefix positions. |
| `SYLLABLE` | Infer nuclei from the configured vowel set; between nonadjacent nuclei retain the longest registered onset, default one consonant. Adjacent vowels are treated as one nucleus. Then apply orthographic vetoes/minima. |

These are data-driven algorithm implementations, not hand-authored complete
English/German/etc. profiles. Morphology does not infer spelling alternations;
use explicit morphemes or the spelling affix API where appropriate. Syllable and
orthographic heuristics do not infer stress, phonemes or pronunciation. Supply
onset inventories, vowel inventories, prefixes/suffixes and forbidden groups
appropriate to the language. Orthographic proposals form a union, with vetoes
applied last; affixes do not silently delete other syllabic proposals.

Liang accepts individual UTF-8 patterns with ASCII weights 0..9 and '.' anchors,
not complete TeX source files, `\patterns` macros, nonstandard replacement
patterns or discretionary spelling changes. For TeX-style exceptions, query
DICTIONARY first and fall back to LIANG on NOMATCH. Patterns are case-sensitive.

## Persistence, dictionaries and limits

`dictionary_load` atomically imports UTF-8 text with one `hy-phen-ated` word per
LF/CRLF line; empty lines are ignored, no comment syntax is inferred. Hyphens
are markers, not part of the resulting key. Literal hyphenated lexical words
can instead be inserted with `add_word` and explicit offsets.

`serialize` atomically appends a complete versioned CRC-protected KVDB/cache
snapshot: `D`+word maps to byte masks, `P`+pattern letters to weights,
`M`+morpheme to flags, and `@rules`, `@fst`, `@dfa`, `@format` carry metadata.
`deserialize` validates it, copies data, and rebuilds HAT-trie/trie/machine
indexes. No native pointers, ABI structs or external rule-file paths are stored.
Use `mulib-cache` to store the snapshot under an application-versioned key and
explicitly `save`/`commit` it to disk. The index backend can differ on reload.

Word and morpheme cap: 4096 UTF-8 bytes; individual pattern-letter cap 4095
bytes, pattern source cap 4096 bytes. Dictionary text imports cap at 16 MiB and
model decoding at 64 MiB. Liang/morphology/finite-state work is bounded at
10000000 engine work units, independently of convention filtering's budget.
Graph decoding also caps outgoing-range validation at 10000000 comparisons.
Status is zero success, positive NOMATCH, or negative INVALID/MEMORY/LIMIT.
Failure preserves existing results and models. CRC is not authentication.

See [`examples/modules/hyphen.c`](../examples/modules/hyphen.c) and
`tests/test_hyphen.c` for all six engines, Unicode offsets, disk roundtrips,
machine composition, even-weight inhibition and morphology.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hyphen.h`](../include/mulib-hyphen.h) |
| Implementation switch | `MULIB_IMPL_HYPHEN` |
| Repository-local includes | [mulib-cache](mulib-cache.md), [mulib-dfa](mulib-dfa.md), [mulib-orhoconv](mulib-orhoconv.md) |
| Complete example | [`examples/modules/hyphen.c`](../examples/modules/hyphen.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hyphen.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
