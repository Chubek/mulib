# mulib-hwinfo

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HWINFO` once before `mulib-hwinfo.h`; no other switches needed.
[Example](../examples/modules/hwinfo.c) prints CPU information without an OS API.

`mulib_hwinfo_cpu` fills a caller-owned struct with vendor/brand strings,
family/model/stepping, raw leaf-1 feature bits and address widths. Unavailable
optional leaves yield zero fields. Strings are always terminated. Required query
failure returns -1; optional brand-query failure may leave a partial result.
Features are CPU capabilities, **not** evidence the OS enabled AVX/XSAVE or NX.

A NULL query uses `mulib_hwinfo_native`, which uses GCC/Clang `<cpuid.h>` on x86
and returns -1 on other targets. Custom queries receive leaf/subleaf and must
fill all four registers (EAX, EBX, ECX, EDX) on zero/success. Only advertised leaves
are queried. No allocations, global state, PCI writes or privileged probes occur.
On legacy i386, CPUID support must be checked by the target environment/toolchain.

This is the CPU inventory component, not a full DMI/SPD inventory program.
Compose it with `mulib-pci.h` for devices, `mulib-efi.h` for memory descriptors,
and `mulib-acpi.h` for firmware tables. Those modules have independent switches.

`mulib_hwinfo_cache` decodes deterministic CPUID cache descriptors: select leaf 4
for Intel or 0x8000001d for supported AMD CPUs, then enumerate subleaf indices.
It reports level/type, line bytes, ways, partitions, sets, shared-thread capacity
and overflow-checked total bytes. Returns 0 for a result, 1 for end of enumeration,
-1 for unsupported/invalid/overflow. Outputs are unchanged on failure. The caller
selects the vendor-appropriate leaf; sharing count is not an online CPU count.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hwinfo.h`](../include/mulib-hwinfo.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hwinfo.h
 * @brief x86 CPUID inventory with injectable queries for hosted testing.
 * Define MULIB_IMPL_HWINFO once. No OS, allocation or privileged instructions.
 */
#ifndef MULIB_HWINFO_H
#define MULIB_HWINFO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_hwinfo_query_t)(void *, uint32_t, uint32_t, uint32_t[4]);
typedef struct {
    char vendor[13], brand[49];
    uint32_t family, model, stepping, features_ecx, features_edx;
    unsigned physical_bits, virtual_bits;
} mulib_hwinfo_cpu_t;
/** @brief Native CPUID on GCC/Clang x86; returns -1 elsewhere. */
int mulib_hwinfo_native(void *context, uint32_t leaf, uint32_t subleaf, uint32_t out[4]);
/** @brief Fill CPU inventory, zeroing unavailable optional fields; query NULL uses native. */
int mulib_hwinfo_cpu(mulib_hwinfo_query_t query, void *context, mulib_hwinfo_cpu_t *out);
typedef struct {
    uint64_t bytes;
    uint32_t line_bytes, ways, partitions;
    uint64_t sets;
    unsigned level, type, sharing_threads;
} mulib_hwinfo_cache_t;
/** @brief Decode deterministic cache CPUID leaf 4 (Intel) or 0x8000001d (AMD).
 * Caller chooses a vendor-appropriate leaf; index is subleaf. 0 result,
 * 1 end, -1 unsupported/invalid/overflow. Output unchanged on failure.
 */
int mulib_hwinfo_cache(mulib_hwinfo_query_t query, void *context, uint32_t leaf,
                        uint32_t index, mulib_hwinfo_cache_t *out);
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
| Header | [`include/mulib-hwinfo.h`](../include/mulib-hwinfo.h) |
| Implementation switch | `MULIB_IMPL_HWINFO` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hwinfo.c`](../examples/modules/hwinfo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hwinfo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
