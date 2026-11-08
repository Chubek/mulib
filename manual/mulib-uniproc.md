# mulib-uniproc

[Manuals](README.md) · [Library index](index.md)

Strict UTF conversion and Unicode normalization, casing and scalar properties.
The Unicode algorithms and database are provided by the installed ICU common
runtime; no ICU development headers or link-time dependency are required.

## Integration

Define `MULIB_IMPL_UNIPROC`, `MULIB_IMPL_UTF8`, and `MULIB_IMPL_DYNLOAD` in one
C11 translation unit, before including `mulib-uniproc.h`. Link `-ldl` where
required by the native loader. See `examples/modules/uniproc.c`.

`mulib_uniproc_open(NULL, 0)` searches native ICU names and unversioned/versioned
symbols (major versions 40–150). An explicit library path and symbol major
override discovery; major zero auto-detects. An unavailable/incompatible library
or allocation failure returns NULL. Close
the context with `mulib_uniproc_close`; serialize calls sharing a context.
The context must outlive calls by bidi, unibreak and uniname.

ICU is pinned with `mulib_dynload_open_pinned`: context close releases its loader
reference, but the module and its shared caches stay mapped until process exit.
Repeated contexts reuse those native caches. Calling process-global `u_cleanup`
from one wrapper context would invalidate other ICU users, so it is never called.
Native module pinning support is required; there is no MuLIB global handle cache.

## Operations and ownership

* `utf16` strictly decodes length-delimited UTF-8, returning a malloc-owned
  terminated UTF-16 array and its unit count. Optional malloc-owned offsets map
  each UTF-16 unit to a UTF-8 byte offset, including an end sentinel. Both units
  of a supplementary scalar have the same starting byte offset.
* `utf8` strictly decodes UTF-16 and returns a malloc-owned terminated UTF-8
  buffer and byte length. Lone surrogates fail. Conversions need no ICU context.
* `normalize` accepts NFC, NFD, NFKC or NFKD. Canonical ordering, composition,
  compatibility mappings and Hangul processing use ICU Normalizer2.
* `case_map` provides locale-sensitive full upper/lower mappings or default
  Unicode case folding (including expansions). NULL locale selects root,
  rather than the process locale. Folding is locale-independent; Turkish
  tailoring is available through lower/upper with locale `tr`.
* `properties` returns ICU `UCharCategory` numeric category, canonical combining
  class and simple one-scalar lower/upper mappings. Any output may be NULL.
* `symbol` exposes version-correct ICU symbol lookup for extension modules.
  A successful address is borrowed until context close, with the host loader's
  function-pointer representation requirements.
* `script` returns the four-letter Unicode Script tag and optional
  Default_Ignorable_Code_Point status. Common and Inherited return `Zyyy` and
  `Zinh`; outputs are unchanged on failure. Font-set shaping uses this to avoid
  treating controls/variation selectors as ordinary nominal-glyph coverage.

All returned buffers use `free`. Lengths exclude terminators and embedded NULs
are preserved. NULL input is accepted only for zero length. Conversion functions
return 0/-1 and leave outputs unchanged on failure. Transformations return NULL
on invalid UTF-8, unavailable symbols, invalid options, native errors or OOM;
output lengths are unchanged on failure. Scalar properties reject surrogates
and values above U+10FFFF. ICU processes noncharacters as valid Unicode scalars.

## Limits and portability

Input byte/unit lengths are limited to `INT32_MAX`; expanded output must also
fit ICU's signed 32-bit capacities. Allocation arithmetic is checked. Unicode
version, locale data and algorithm behavior track the runtime ICU build; this
module does not embed a frozen Unicode database. Platforms need a compatible
native dynamic loader and ICU C ABI. Builds without ICU still compile and can
use strict UTF conversion; Unicode operations report unavailability.

Tests: `tests/test_unicode_text.c`, `tests/test_text_allocations.c` and
`python3 tools/check_text.py` (optional native ABI comparisons).

ABI reference: <https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/>.
The wrapper is original code; no ICU implementation or data is vendored.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uniproc.h`](../include/mulib-uniproc.h) |
| Implementation switch | `MULIB_IMPL_UNIPROC` |
| Repository-local includes | [mulib-dynload](mulib-dynload.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/uniproc.c`](../examples/modules/uniproc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uniproc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
