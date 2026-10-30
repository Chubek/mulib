# ipfs — canonical content identifiers

Define `MULIB_IMPL_VARINT` and `MULIB_IMPL_IPFS`.
See [`examples/modules/ipfs.c`](../examples/modules/ipfs.c).

The supported CID profile is version 1, raw (0x55) or dag-pb (0x70) codec,
sha2-256 multihash (0x12), 32-byte digest. The public value copies codec and
digest. Binary encoding is exactly 36 bytes with minimal varints; binary decode
requires that exact canonical representation. Text uses lowercase base32
multibase prefix `b`, exactly 59 characters, with unused final bits zero.
Format writes 60 bytes including NUL. CIDv0, base58, other hashes/codecs and
noncanonical encodings are rejected rather than normalized.

All codecs return 1 success or 0 invalid. Caller output remains unchanged on
failure. `verify` delegates actual SHA-256 to a caller callback, then compares
all 32 digest bytes; return 1 only on a matching block. The hash callback must
return 1 and fill the digest, and receives borrowed context/data. Encoding a
CID alone does not establish that any supplied bytes have that content hash.

No allocations, block store, daemon, DHT, Bitswap, DAG traversal or network
fetches are implicit. Compose identifiers with transport/storage APIs as needed.
Tests include the independently known CID of an empty raw block and reject
incorrect trailing base32 bits. See `tests/test_protocol_extensions.c`.
