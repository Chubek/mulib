# Auxiliary hashes

Define `MULIB_IMPL_AUXHASH` once. The module provides FNV-1 and FNV-1a (32/64),
DJB2/DJBX33A, DJBX33X, SDBM, DEK, ELF/PJW, RS, JS, AP, BKDR, Jenkins one-at-a-time,
lookup3 (`hashlittle`), MurmurHash2, MurmurHash64A, MurmurHash3 x86-32,
Knuth multiplicative integer hashes, and configurable 64-bit BuzHash.
These functions provide no cryptographic collision resistance.

Byte APIs accept arbitrary binary data and explicit byte lengths. NULL input
is permitted at zero length; a NULL input with nonzero length returns zero.
Unsigned arithmetic gives reproducible wraparound, and word loads explicitly
use little endian. Unaligned buffers are supported. Seeds are explicit for
lookup3 and Murmur hashes. DJB2 and DJBX33A are aliases of the additive recurrence
starting at 5381; DJBX33X uses XOR. ELF and 32-bit PJW give the same result.
Knuth32 multiplies by 2654435761; Knuth64 uses 11400714819323198485.
These return the whole product; choose bucket bits or reduce it separately.

Robin Hood describes a hash-table collision/probing strategy, rather than a
hash function. These hashes can supply its initial bucket index. Store probe
distances and implement Robin Hood insertion in the table itself.

BuzHash requires a caller-owned 256-entry table. `mulib_auxhash_buzhash_table`
fills it reproducibly from a seed using SplitMix64. There is no single standard
BuzHash byte table; the table is part of the hash definition and must be shared
by all producers/consumers. For a nonempty fixed-size window, calculate its hash
with `mulib_auxhash_buzhash`, then slide by removing the outgoing byte and adding
the incoming byte with `mulib_auxhash_buzhash_roll`. Window sizes divisible by
64 are supported. An invalid table or zero rolling window returns zero.

The older `mulib-hash.h` FNV-1a API remains compatible. No allocation occurs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-auxhash.h`](../include/mulib-auxhash.h) |
| Implementation switch | `MULIB_IMPL_AUXHASH` |
| Repository-local includes | [`include/detail/mulib-hash-core.h`](../include/detail/mulib-hash-core.h) |
| Complete example | [`examples/modules/auxhash.c`](../examples/modules/auxhash.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/auxhash.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
