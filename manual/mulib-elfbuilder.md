# ELF builder

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ELFBUILDER`, `OBJFILEBUILDER`, `OBJFILEWRITER`, `BINFILEWRITER`,
`BYCUTILS` and `BUFFER`. Reuses owned section accumulation and atomic ELF64
little-endian ET_REL writing. Names/data are copied; flags/alignment follow
objfilebuilder. No symbols/relocations/executable segments.
[Example](../examples/modules/elfbuilder.c), [contracts](storage-server.md).

`finish_abi` adds named little-endian ELF32/64 ET_REL profiles for Linux SysV
x86-64, AAPCS64, RV64 LP64D, FreeBSD x86-64, and **all four current Android ABIs**:
armeabi-v7a, arm64-v8a, x86 and x86_64. `abi_info` reports machine, class, OSABI,
e_flags, C stack alignment and long-double representation bits. Android ELF uses
OSABI_NONE (0), not a fictitious Android OSABI; ARM selects EABI5 soft-float
argument convention, and arm64 records Android's reserved x18. The source facts
are in `standards/android/android-abi.txt`. Linux x86-64's 80-bit long-double
representation may occupy a larger padded storage slot; this is not sizeof.

ELF32 offsets/section sizes/alignments must fit 32 bits. All profiles atomically
append, reject unknown ABI IDs and preserve existing output on failure. The
historical `finish(machine)` remains ELF64 and unchanged. Profiles describe object
headers and target conventions: they do not generate call sequences, Android
notes, symbols, relocations, shared libraries, an interpreter path or runnable
segments. Android's 16-KiB shared-library segment policy is not applicable to these
ET_REL objects. Tests use independent readelf/LLVM readers in `tools/check_android.py`.
`tests/test_android_abi_alloc.c` enumerates each allocation failure for ELF32/64
and the other new builder profile paths, checking cleanup and atomic prefixes.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-elfbuilder.h`](../include/mulib-elfbuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-elfbuilder.h
 * @brief Owned ELF64 relocatable section builder reusing objfilebuilder.
 * Define MULIB_IMPL_ELFBUILDER and OBJFILEBUILDER dependencies.
 * No symbols, relocations or executable segments; see objfilewriter profile. */
#ifndef MULIB_ELFBUILDER_H
#define MULIB_ELFBUILDER_H
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_objfilebuilder_t mulib_elfbuilder_t;
  int mulib_elfbuilder_add (mulib_elfbuilder_t *builder, const char *name,
                            const void *data, size_t size, size_t alignment,
                            uint64_t flags);
  int mulib_elfbuilder_finish (const mulib_elfbuilder_t *builder,
                               uint16_t machine, mulib_buffer_t *out);
  void mulib_elfbuilder_free (mulib_elfbuilder_t *builder);
  typedef enum mulib_elfbuilder_abi {
    MULIB_ELFBUILDER_LINUX_X86_64, MULIB_ELFBUILDER_LINUX_AARCH64,
    MULIB_ELFBUILDER_LINUX_RISCV64, MULIB_ELFBUILDER_FREEBSD_X86_64,
    MULIB_ELFBUILDER_ANDROID_ARMEABI_V7A, MULIB_ELFBUILDER_ANDROID_ARM64_V8A,
    MULIB_ELFBUILDER_ANDROID_X86, MULIB_ELFBUILDER_ANDROID_X86_64
  } mulib_elfbuilder_abi_t;
  typedef struct mulib_elfbuilder_abi_info {
    const char *name;
    uint16_t machine;
    unsigned bits, osabi, stack_alignment, long_double_bits;
    uint32_t flags;
    int reserved_x18;
  } mulib_elfbuilder_abi_info_t;
  /** Query a named little-endian C ABI. Android uses ELFOSABI_NONE, not a
   * fictitious Android OSABI. This is format metadata, not call lowering. */
  int mulib_elfbuilder_abi_info (mulib_elfbuilder_abi_t abi,
      mulib_elfbuilder_abi_info_t *out);
  /** Emit ELF32/64 ET_REL with the ABI's machine, OSABI and e_flags. No loader,
   * dynamic linker, symbols or relocations are implied. Atomic append. */
  int mulib_elfbuilder_finish_abi (const mulib_elfbuilder_t *builder,
      mulib_elfbuilder_abi_t abi, mulib_buffer_t *out);
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
| Header | [`include/mulib-elfbuilder.h`](../include/mulib-elfbuilder.h) |
| Implementation switch | `MULIB_IMPL_ELFBUILDER` |
| Repository-local includes | [mulib-binfilewriter](mulib-binfilewriter.md), [mulib-bycutils](mulib-bycutils.md), [mulib-objfilebuilder](mulib-objfilebuilder.md) |
| Complete example | [`examples/modules/elfbuilder.c`](../examples/modules/elfbuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/elfbuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
