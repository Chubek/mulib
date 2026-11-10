# Checksums

Define `MULIB_IMPL_CHECKSUM` once. The module implements Adler-32,
Fletcher-16/32/64, Internet one's-complement checksum, BSD rotating checksum,
System V checksum, modulo 8/16/32/64-bit sums of bytes, XOR of bytes, and Luhn
check digits. These are not cryptographic message authenticators.

Use a named one-shot function or `mulib_checksum_init`, repeated
`mulib_checksum_update`, and `mulib_checksum_final`. Status functions return 1/0;
final is a repeatable snapshot, so updates may continue afterward. All lengths
are bytes. NULL input is valid only for zero length. Invalid input to a named
checksum returns zero. A valid checksum can also be zero. No allocation occurs.

Fletcher-16 sums bytes modulo 255. Fletcher-32 sums big endian 16-bit words
modulo 65535, and Fletcher-64 sums big endian 32-bit words modulo 4294967295.
Each returns `(second_sum << word_bits) | first_sum`, starts with zero sums,
and zero-pads incomplete final words in the low bytes. The Internet checksum
also uses big endian 16-bit words and pads an odd final byte with a low zero.
Its empty result is 0xffff. Updates retain partial words across calls, so odd
splits preserve the one-shot result. BSD rotates its 16-bit accumulator right
one bit before adding each byte. System V folds the 32-bit byte sum twice.

The Adler API is named `mulib_checksum_adler32` to coexist with the existing
`mulib-adler32.h`. `mulib_checksum_adler32_update` accepts an already completed
Adler result as seed; start with seed 1. Results match the existing module.

`mulib_luhn_digit` computes the digit to append to nonempty decimal ASCII and
returns -1 for invalid input. `mulib_luhn_valid` validates a string including
its check digit and returns 1/0. Embedded NUL and other nondigits are rejected.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-checksum.h`](../include/mulib-checksum.h) |
| Implementation switch | `MULIB_IMPL_CHECKSUM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/checksum.c`](../examples/modules/checksum.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/checksum.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
