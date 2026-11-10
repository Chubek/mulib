# CRCs

Define `MULIB_IMPL_CRC` once. This header supplies a byte-oriented, parameterized
CRC engine for widths 1 through 128, plus all 113 canonical models in the
CRC RevEng catalogue retrieved on November 10, 2026. It includes CRC-3 through
CRC-82 variants, IEEE CRC-32, CRC-32C/iSCSI, CRC-64/ECMA-182, CRC-64/XZ,
and their catalogue aliases under canonical names. CRCs detect accidental
corruption and do not authenticate messages.

`mulib_crc_catalogue` holds names, widths, polynomials, initial values, reflection
flags, final XORs, and published check values for ASCII `123456789`.
`mulib_crc_find` matches exact canonical names (for example `CRC-32/ISO-HDLC`)
and returns NULL for an unknown name. Enumerators such as
`MULIB_CRC_32_ISO_HDLC` index the same table. Named functions such as
`mulib_crc_32_iso_hdlc` compute individual catalogue CRCs.

For custom models, fill `mulib_crc_params`. Polynomials omit the leading
x^width term and use the normal, unreflected representation. Multiword values
are `{lo, hi}`; bits above the selected width must be zero. Reflection flags
must be 0 or 1. The engine processes reflected input bytes least-significant
bit first and other input bytes most-significant bit first. The register is
always stored unreflected; `refout` reflects it before applying `xorout`.

Use `mulib_crc_compute` for a one-shot calculation, or `mulib_crc_init`, repeated
`mulib_crc_update`, and `mulib_crc_final` for streaming. Status APIs return 1/0.
Finalization is a snapshot and can be repeated or followed by more updates.
NULL input is valid only at zero length. The named functions return zero for
invalid input, which is also a possible valid CRC. Named results through 64
bits return `uint64_t`; CRC-82/DARC returns `mulib_crc_value`. No allocation
occurs. Serialization order is the caller's responsibility.

`mulib_crc32c_intrinsics(seed, data, length)` uses SSE4.2 CRC instructions when
compiled with `-msse4.2` and otherwise uses a portable CRC-32C implementation.
Use seed zero initially and pass a completed result as seed to continue.
Compiler target features must match the CPU; there is no runtime dispatch.
`MULIB_HASH_NO_INTRINSICS` forces the portable path. The existing IEEE
`mulib-crc32.h` API remains separate and unchanged; SSE4.2 implements CRC-32C,
not IEEE CRC-32.

Tests verify every preset against its published check value, every split of the
check message, and IEEE/CRC-32C interoperability with the existing API and
intrinsic implementation.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-crc.h`](../include/mulib-crc.h) |
| Implementation switch | `MULIB_IMPL_CRC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/crc.c`](../examples/modules/crc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/crc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
