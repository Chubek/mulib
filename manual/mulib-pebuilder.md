# PE builder

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PEBUILDER`, `MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_OBJFILEBUILDER`.
Atomically appends a fixed-base AMD64 PE32+ console image with one .text section.
Entry offset must be inside nonempty code. No imports/runtime/unwind tables/
relocations/signing; freestanding position-independent code only.
[Example](../examples/modules/pebuilder.c), [contracts](storage-server.md).

The owned multi-section API uses zero-initialized `mulib_pebuilder_t` and
`add`/`finish`/`free`. Add copies unique 1..8-byte names and payloads; alignment
is a power of two <=4096, flags are shared WRITE/ALLOC/EXEC bits 1/2/4. Names
starting with `/` are rejected. `finish(builder, 0x8664, out)` writes an AMD64
PE32+ **data-only DLL profile with no entry point**, not a runnable application.
Use `text` for a freestanding executable. Up to 96 sections, 512-byte file
alignment and 4096-byte section alignment; file/image offsets must fit 32 bits.
Every section is raw initialized data (including any caller-supplied zeroes).
All writes append atomically, returning 1/0. Finish does not consume builder.
See [ownership and format contracts](binary-formats.md).

`text_abi` selects Windows x64, Windows ARM64, UEFI x64 or UEFI ARM64 PE32+
profiles. Machine is AMD64/ARM64; subsystem is Windows console (3) or EFI
application (10). ARM64 code length and entry must be four-byte aligned. Existing
text/finish APIs retain their AMD64 defaults. `finish_abi` supports the two Windows
data-only DLL profiles; UEFI requires text_abi and an entry. Unknown ABI IDs are
rejected without output changes. These selectors do not synthesize a Windows
runtime, EFI entry prototype, imports, call ABI lowering or unwind metadata.
`tests/test_android_abi.c` and independent llvm-readobj checks cover all profiles.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pebuilder.h`](../include/mulib-pebuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pebuilder.h
 * @brief Minimal deterministic fixed-base AMD64 PE32+ console image.
 * Define MULIB_IMPL_PEBUILDER plus OBJFILEBUILDER, OBJFILEWRITER,
 * BYCUTILS, BINFILEWRITER and BUFFER.
 * Single read/execute .text, no imports, relocations, unwind tables or signing.
 * Caller supplies freestanding position-independent code, not a C runtime.
 * Atomic append, 1 success/0 invalid/OOM/overflow. */
#ifndef MULIB_PEBUILDER_H
#define MULIB_PEBUILDER_H
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_pebuilder_text (mulib_buffer_t *out, const void *code,
                            size_t length, size_t entry_offset);
  typedef mulib_objfilebuilder_t mulib_pebuilder_t;
  /** @brief Copy a 1..8-byte named section; power-of-two alignment <=4096,
   * flags WRITE/ALLOC/EXEC = 1/2/4. Zero initialize builder. */
  int mulib_pebuilder_add (mulib_pebuilder_t *b, const char *name,
      const void *data, size_t size, size_t alignment, uint64_t flags);
  /** @brief Append AMD64 PE32+ data image with no entry/imports/signature.
   * machine must be 0x8664. Use text() for a freestanding executable. */
  int mulib_pebuilder_finish (const mulib_pebuilder_t *b, uint16_t machine, mulib_buffer_t *out);
  /** @brief Release all builder storage. */
  void mulib_pebuilder_free (mulib_pebuilder_t *b);
  typedef enum mulib_pebuilder_abi {
    MULIB_PEBUILDER_WINDOWS_X64, MULIB_PEBUILDER_WINDOWS_ARM64,
    MULIB_PEBUILDER_UEFI_X64, MULIB_PEBUILDER_UEFI_ARM64
  } mulib_pebuilder_abi_t;
  /** Emit a freestanding PE32+ image with Windows console or EFI application
   * subsystem and matching machine. No imports/unwind/relocations supplied. */
  int mulib_pebuilder_text_abi (mulib_buffer_t *out, const void *code,
      size_t length, size_t entry_offset, mulib_pebuilder_abi_t abi);
  /** Emit a Windows data-only DLL; UEFI profiles require text_abi and an entry. */
  int mulib_pebuilder_finish_abi (const mulib_pebuilder_t *b,
      mulib_pebuilder_abi_t abi, mulib_buffer_t *out);
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
| Header | [`include/mulib-pebuilder.h`](../include/mulib-pebuilder.h) |
| Implementation switch | `MULIB_IMPL_PEBUILDER` |
| Repository-local includes | [mulib-binfilewriter](mulib-binfilewriter.md), [mulib-bycutils](mulib-bycutils.md), [mulib-objfilebuilder](mulib-objfilebuilder.md) |
| Complete example | [`examples/modules/pebuilder.c`](../examples/modules/pebuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pebuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
