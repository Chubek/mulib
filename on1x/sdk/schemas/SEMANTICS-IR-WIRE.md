# SDK semantics/IR wire schemas

The JSON schemas describe the logical records. The implemented wire encodings
are deterministic little-endian, length-delimited binary envelopes. No pointer,
allocator, callback, context token, or validation authority is serialized.

## Common scalar encoding

- `u32`, `u64`: unsigned little-endian integer.
- `text`: `u32` byte count followed by canonical UTF-8 bytes. Embedded NUL is
  rejected.
- Inputs are capped at 16 MiB, 262144 HIR nodes, 256 nesting levels, and an
  explicit work budget.

## `O1XHIR1\0`

The HIR header is magic, schema `u32` (`1`), reserved zero `u32`, generation
`u64`, revision `u64`, provenance `text`, node count `u64`, function-root count
`u64`, then root IDs. Each node stores ID, class, kind, flags, name, source
point, recursive canonical type, labels, four auxiliary integers, and typed
child edges.

HIR-v1 is an inspection schema for monomorphic elaborated function trees.
Serialization rejects retained declarations or metadata it cannot preserve.
Deserialization validates one rooted acyclic tree and always returns
`CANDIDATE`; it cannot recreate the private canonical `Unit`.

## `O1XMIR1\0`

The MIR envelope is magic, schema `u32` (`1`), canonical MIR version `u32`,
generation `u64`, revision `u64`, provenance `text`, payload length `u64`, and
the existing canonical `on1x.mir` S-expression. The payload decoder and
canonical MIR verifier both run. Deserialization nevertheless returns
`CANDIDATE`; structural validity is not publication or equivalence evidence.

Generation and revision in an envelope must exactly match decode options.
Owner context is process-local and supplied separately by the host.
