# mulib-imgformat

## NAME

Version-1 container image envelope and complete process-spec wire codec.

## INTEGRATION

Define `MULIB_IMPL_IMGFORMAT`, `MULIB_IMPL_CONTAINER`, `MULIB_IMPL_CRC32` once.
Example: `examples/modules/imgformat.c`. All wire integers are little-endian;
no native structs, padding or pointers are serialized.

## IMAGE ENVELOPE

The fixed 64-byte header is followed by configuration bytes then rootfs bytes:

| Offset | Field |
| --- | --- |
| 0 | Eight-byte `MULIMG1\0` magic |
| 8 | u32 version, exactly 1 |
| 12 | u32 flags, zero |
| 16 | u64 configuration length |
| 24 | u64 rootfs length |
| 32 | IEEE CRC32 of concatenated config/rootfs payload |
| 36 | IEEE CRC32 over header bytes 0–35 then 40–63 |
| 40 | u32 architecture: 0 portable, 1 AMD64, 2 ARM64, 3 OIX, 4 QVM |
| 44 | Twenty zero reserved bytes |

`pack` encodes a validated process spec and borrows supplied rootfs bytes during
the call. `open` checks magic, version, flags, reserved bytes, exact lengths,
architecture, quotas and both CRCs, publishing a borrowed view only on success.
Configuration length is capped at 1 MiB. Rootfs grammar and configuration content
are validated separately by their respective decoders. CRC detects accidental
corruption; authentication belongs to the application/transport.

## PROCESS RECORD

The 64-byte config prefix contains `MCFG`, u32 version 1, u32 argc/envc, UID/GID,
read-only (0/1), zero reserved u32, then u64 memory/CPU/deadline and u32 process/
open-file requests. It is followed by ID, cwd, argv and env strings, each with a
u32 byte length and non-NUL contents. Lengths exclude terminators. Empty arguments
and values are preserved; embedded NULs and trailing bytes are rejected.

`spec_encode` caps total strings at 4096 and output at 1 MiB. `spec_decode`
validates ID, canonical cwd, environment names and duplicate names, creating
owned NUL-terminated strings with 4096-string/1-MiB decoding quotas. Allocation
quotas are local policy, not serialized fields. UID/GID and all resource fields
round-trip without native-size dependence.

## OWNERSHIP AND ERRORS

Pack/encode replace initialized blobs atomically and allow input to alias the old
output. Decode constructs an uninitialized owned spec; open returns borrowed
ranges valid only while original image bytes remain unchanged. Bounds/limits
produce ELIMIT, malformed fields EFORMAT, CRC mismatch ECHECKSUM, allocation
failure ENOMEM. See [image2container](mulib-image2container.md) for full import.
