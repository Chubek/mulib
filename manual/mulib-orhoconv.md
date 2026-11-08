# mulib-orhoconv

[Manuals](README.md) · [Library index](index.md)

An independent orthographic-convention filter; the requested filename spelling
is retained. Define `MULIB_IMPL_ORHOCONV`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_KVDB`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_CRC32` and
`MULIB_IMPL_VARINT` in one C11 translation unit.

`create(left_min, right_min)` owns a KVDB rule store. Minima count Unicode
scalars, not bytes; zero is allowed. Default vowel inventory is
`aeiouyAEIOUY`; vowel inventories are capped at 256 UTF-8 bytes.
`set_minima`, `set_vowels`, `add_token` and `forbid` configure
rules. Tokens are explicit valid UTF-8 onset groups, prefixes or suffixes.
Nothing is silently normalized or lowercased; callers supply language data.

`forbid(rules, "ph", 1)` prevents the split `p-h` anywhere that literal group
occurs. The offset is a UTF-8 **byte boundary** strictly inside the group.
Replacing the same group replaces its forbidden offset. Use larger distinct
contexts for different positional prohibitions. These filters veto candidate
boundaries; they do not invent hyphenation opportunities.

## Filtering and data

`filter(rules, word, bytes, mask)` takes a candidate mask of `bytes+1` bytes.
Nonzero `mask[i]` proposes a break before byte i. Filtering atomically:

1. validates non-NUL UTF-8, word length <=4096 bytes;
2. clears endpoints, UTF-8 interiors and positions violating scalar minima;
3. clears boundaries before common combining-mark/variation-selector ranges,
   and adjacent to U+200D ZWJ;
4. applies every forbidden literal-group split.

This is not complete Unicode grapheme segmentation. Normalize/case-fold and
apply any script-specific segmentation policy before calling. Results preserve
original UTF-8 byte indexing. A malformed input/allocation/work-limit error
leaves the mask unchanged. Return codes are KVDB results; EOVERFLOW reports a
rule/work limit (4096 records, 10000000 forbidden-pattern comparisons).

`is_vowel` and `has_token` support syllabifiers. `database` borrows a **read-only**
KVDB view. Its portable schema is: `V` vowel bytes, `L` two LE64 minima,
`O/P/S` plus UTF-8 token with an empty value, and `F` plus group with an LE64
forbidden offset. Do not mutate the borrowed database through a cast.

`serialize`/`deserialize` use the versioned CRC-protected KVDB format, validating
all keys, lengths and boundary positions. Deserialization owns its data and
leaves output untouched on failure. `destroy` accepts NULL. No language rule
files or dictionaries are bundled or implicitly loaded.

See [`examples/modules/orhoconv.c`](../examples/modules/orhoconv.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-orhoconv.h`](../include/mulib-orhoconv.h) |
| Implementation switch | `MULIB_IMPL_ORHOCONV` |
| Repository-local includes | [mulib-endian](mulib-endian.md), [mulib-kvdb](mulib-kvdb.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/orhoconv.c`](../examples/modules/orhoconv.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/orhoconv.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
