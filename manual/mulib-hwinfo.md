# mulib-hwinfo

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
