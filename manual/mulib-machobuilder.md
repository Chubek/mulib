# Mach-O builder

[Manuals](README.md) · [Library index](index.md)

Include the canonical `mulib-machobuilder.h`. Define `MULIB_IMPL_MACHOBUILDER`,
`MULIB_IMPL_OBJFILEBUILDER`, `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_BINFILEWRITER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`. The canonical switch also enables the
historical `mulib_machobuilder_text` convenience function; either entry header's
switch can enable that function without duplicate definitions when both are set.
[Example](../examples/modules/machobuilder.c).

Zero-initialize `mulib_machobuilder_t`. `add` copies unique 1..16-byte names and
arbitrary payloads, accepting NULL only for zero-size payloads. Alignment must
be a power of two at most 32768. Flags are shared WRITE/ALLOC/EXEC bits 1/2/4;
EXEC sections are named in `__TEXT`, other sections in `__DATA`. Mach-O segment
permissions are a permissive object-file profile, not an executable memory policy.

`finish(builder, cpu, out)` atomically appends a deterministic little-endian
Mach-O64 MH_OBJECT. CPU is x86-64 `0x01000007` or arm64 `0x0100000c`.
There is one unnamed LC_SEGMENT_64, at most 255 sections, no symbols, relocations,
entry command, dyld metadata or code signature. File offsets/total size must fit
32 bits. This is not a runnable macOS image.

Functions return 1/0; failures leave output unchanged. `finish` does not consume
the builder; `free` resets it. Output is an ordinary owned `mulib_buffer_t`.
The historical misspelled header/switch remain valid and unchanged for the
single-text-section API. See [shared contracts](binary-formats.md).

`finish_abi` adds macOS x86-64/arm64, iOS arm64 and iOS-simulator x86-64/arm64
profiles. It emits the correct CPU/subtype and an LC_BUILD_VERSION command with
platform macOS=1, iOS=2 or iOS simulator=7. min_os/sdk use major<<16|minor<<8|patch;
min_os must be nonzero and a specified sdk cannot predate min_os. Callers select
deployment versions valid for their actual target/toolchain. Section data offsets
are recomputed after the additional command, not patched without relocating data.
No arm64e/pointer-authenticated ABI, dylib linking, dyld entry or native call
lowering is implied. Legacy finish omits LC_BUILD_VERSION as before. Independent
LLVM format-reader checks are in `tools/check_android.py`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-machobuilder.h`](../include/mulib-machobuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-machobuilder.h
 * @brief Owned multi-section little-endian Mach-O64 MH_OBJECT builder.
 * Define MULIB_IMPL_MACHOBUILDER and OBJFILEBUILDER dependencies.
 * Also enables the historical machobuidler text API without redefining its
 * implementation switch; both entry headers can coexist in one TU. */
#ifndef MULIB_MACHOBUILDER_H
#define MULIB_MACHOBUILDER_H
#include "mulib-objfilebuilder.h"
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include "mulib-machobuidler.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_objfilebuilder_t mulib_machobuilder_t;
/** @brief Copy a 1..16-byte section name; alignment <=32768, flags 1/2/4.
 * EXEC sections use __TEXT, others __DATA. Zero initialize builder. */
int mulib_machobuilder_add (mulib_machobuilder_t *b, const char *name,
    const void *data, size_t size, size_t alignment, uint64_t flags);
/** @brief Append x86-64 (0x01000007) or arm64 (0x0100000c) object atomically.
 * No symbols, relocations, executable entry or signatures. */
int mulib_machobuilder_finish (const mulib_machobuilder_t *b, uint32_t cpu, mulib_buffer_t *out);
/** @brief Release copied names/bytes. */
void mulib_machobuilder_free (mulib_machobuilder_t *b);
typedef enum mulib_machobuilder_abi {
  MULIB_MACHOBUILDER_MACOS_X86_64, MULIB_MACHOBUILDER_MACOS_ARM64,
  MULIB_MACHOBUILDER_IOS_ARM64, MULIB_MACHOBUILDER_IOS_SIM_X86_64,
  MULIB_MACHOBUILDER_IOS_SIM_ARM64
} mulib_machobuilder_abi_t;
/** Append MH_OBJECT with CPU/subtype and LC_BUILD_VERSION platform. min_os and
 * sdk use packed major<<16|minor<<8|patch; zero SDK means unspecified. */
int mulib_machobuilder_finish_abi (const mulib_machobuilder_t *b,
    mulib_machobuilder_abi_t abi, uint32_t min_os, uint32_t sdk, mulib_buffer_t *out);
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
| Header | [`include/mulib-machobuilder.h`](../include/mulib-machobuilder.h) |
| Implementation switch | `MULIB_IMPL_MACHOBUILDER` |
| Repository-local includes | [mulib-binfilewriter](mulib-binfilewriter.md), [mulib-bycutils](mulib-bycutils.md), [mulib-machobuidler](mulib-machobuidler.md), [mulib-objfilebuilder](mulib-objfilebuilder.md) |
| Complete example | [`examples/modules/machobuilder.c`](../examples/modules/machobuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/machobuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
