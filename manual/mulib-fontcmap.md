# mulib-fontcmap

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTCMAP` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Checked OpenType Unicode cmap lookup and variation sequences.

## API

```c
typedef struct mulib_fontcmap_map { mulib_typecore_slice_t format12; } mulib_fontcmap_map_t;
int mulib_fontcmap_open_format12(mulib_fontcmap_map_t *out,const void *table,size_t length);
int mulib_fontcmap_glyph(const mulib_fontcmap_map_t *map,uint32_t scalar,uint32_t *glyph);
```

## Behavior and limitations

The bounded subset follows **OpenType 1.9.1** cmap formats 4, 6, 10, 12 and 13,
plus format 14 default/non-default Unicode variation sequences. This is not a
shaper or a complete font validator. Non-Unicode legacy encodings and formats
0/2/8 are not interpreted.

`open_format12` retains its ABI and restriction to format 12. `open_subtable`
accepts one of the supported base subtables. `open` accepts an entire cmap table
and deterministically selects a Unicode-platform or Windows Unicode subtable,
preferring formats 12, 13, 10, 4, then 6; at equal format the Unicode platform wins.
The borrowed `format12` field keeps its historical name and unchanged layout,
even when it holds another format. Storage must remain immutable and alive.

`glyph` returns zero for an unmapped scalar, including zero entries in a format-4
glyph array (the delta is not applied to those entries). Invalid scalars,
truncation, malformed range offsets, overlapping base ranges and format-12 glyph
arithmetic overflow fail. `variation` accepts a separately extracted format-14
subtable and a valid FE00..FE0F/E0100..E01EF selector: default entries consult the
base map, non-default entries return their glyph, and absent sequences return
glyph zero. Format-14 validation caps selectors at 256 and limits total inspected
mapping-record bytes to the subtable length; excessive aliased-table work fails.
Base-table selection validates only the chosen subtable, once, rather than
revalidating data shared by thousands of encoding records. APIs return 1/0 and
preserve outputs on failure. See the header for
the additive declarations and `tests/test-fontcmap.c` for all six format fixtures.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontcmap.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontcmap.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontcmap.h`](../include/mulib-fontcmap.h) |
| Implementation switch | `MULIB_IMPL_FONTCMAP` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontcmap.c`](../examples/mulib-fontcmap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontcmap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
