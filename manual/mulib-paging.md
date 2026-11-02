# mulib-paging

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
