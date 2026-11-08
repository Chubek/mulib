# mulib-paging

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PAGING` once; no dependencies, allocations or privileged code.
[Example](../examples/modules/paging.c).

`mulib_paging_canonical` checks sign extension for 48- or 57-bit linear addresses;
other widths return false. `mulib_paging_index` extracts 9-bit index at level
1 (PT) through 5 (PML5); invalid levels return `UINT32_MAX`.

`mulib_paging_entry` accepts a 4 KiB-aligned physical address, CPUID-derived
physical width 12..52, and only low 12 flag bits plus NX. It returns zero and
writes the caller's PTE on success, otherwise -1 without modifying output.
These are **leaf 4 KiB PTE** flags: bit 7 is PAT, not a huge-page request.
Present/write/user/NX macros are supplied; callers may pass the other legal
low flags explicitly. NX requires EFER.NXE; PAT/cache permissions and physical
width remain caller policy. Encryption/key bits outside this profile are rejected.

No page allocator, huge-page builder, CR3 load or TLB shootdown is
included. The application must build table hierarchies, reserve their storage,
validate permissions at every level, enable CPU features and synchronize CPUs.

`mulib_paging_translate` walks existing four/five-level tables through a physical
table-read callback. It handles 4 KiB, 2 MiB and 1 GiB leaves, validates address
width/alignment/reserved physical bits and combines write/user/NX restrictions
across levels. Returns 0 mapped, 1 not present, -1 malformed; callback errors
propagate. Result is written only on success. Root is an aligned table address,
not raw CR3 with PCID bits. The caller stabilizes mappings and checks CPU support
for NX/large pages. Results describe page-table permissions, not additional
CR0.WP, SMEP, SMAP, protection-key or virtualization permissions.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-paging.h`](../include/mulib-paging.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-paging.h
 * @brief Pure x86-64 four/five-level page address and 4 KiB PTE helpers.
 * Define MULIB_IMPL_PAGING once. Does not allocate tables or modify CR3/TLBs.
 */
#ifndef MULIB_PAGING_H
#define MULIB_PAGING_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_PAGING_PRESENT UINT64_C(1)
#define MULIB_PAGING_WRITE UINT64_C(2)
#define MULIB_PAGING_USER UINT64_C(4)
#define MULIB_PAGING_NX (UINT64_C(1) << 63)
/** @brief True for a canonical 48- or 57-bit virtual address. */
int mulib_paging_canonical(uint64_t address, unsigned bits);
/** @brief Index at level 1..5; UINT32_MAX for invalid level. */
uint32_t mulib_paging_index(uint64_t address, unsigned level);
/** @brief Construct 4 KiB leaf PTE, rejecting alignment, physical width and flags.
 * bits is CPUID physical width 12..52; NX availability is caller responsibility.
 */
int mulib_paging_entry(uint64_t physical, unsigned bits, uint64_t flags, uint64_t *entry);
typedef int (*mulib_paging_read_t)(void *, uint64_t table_physical, unsigned index, uint64_t *entry);
typedef struct { uint64_t physical, page_bytes; unsigned writable, user, executable; } mulib_paging_translation_t;
/** @brief Walk existing 4/5-level tables via a physical-read callback. Supports
 * 4 KiB, 2 MiB and 1 GiB leaves; combines permissions across all levels.
 * 0 mapped, 1 not present, -1 malformed, callback errors propagate.
 * Caller stabilizes tables and checks CPU huge-page/NX feature availability.
 */
int mulib_paging_translate(mulib_paging_read_t read, void *context, uint64_t root,
                           unsigned levels, unsigned physical_bits, uint64_t address,
                           mulib_paging_translation_t *result);
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
| Header | [`include/mulib-paging.h`](../include/mulib-paging.h) |
| Implementation switch | `MULIB_IMPL_PAGING` |
| Repository-local includes | None |
| Complete example | [`examples/modules/paging.c`](../examples/modules/paging.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/paging.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
