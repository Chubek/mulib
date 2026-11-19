# On1x SDK ABI 2 contract

This document is normative for SDK 2 implementation work. Public headers remain
C11-compatible and independently includable; C++ wrappers are adapters over
this ABI.

## Descriptor prefix and negotiation

Every extensible structure begins with:

```c
typedef struct On1xSDK_ABIHeader {
    uint32_t abi_version;
    uint32_t struct_size;
} On1xSDK_ABIHeader;
```

SDK 2 descriptors set `abi_version` to `2` and `struct_size` to the size known
to the caller. A callee:

1. reads only the two prefix fields first;
2. rejects a structure shorter than its required prefix;
3. reads only `min(caller_size, known_size)`;
4. treats missing optional trailing fields as zero;
5. ignores unknown trailing fields only within a compatible major ABI.

ABI-v1 descriptor layouts and numeric status values remain accepted through
explicit adapters. A v1 descriptor is never read as a v2 layout.

## Host function table

Extensions receive a borrowed, immutable, versioned `On1xSDK_Host` table. The
table has the ABI prefix, host feature bits, context token, allocator, logging,
diagnostic, cancellation, budget, handle, and candidate-submission operations.
Each function is callable only when its field lies within `struct_size` and is
non-null. Extensions must not resolve or call private compiler symbols.

Host operations return `On1xSDK_Status`; no C++ exception crosses the boundary.
An unknown callback status is converted to `ON1X_SDK_ERROR`.

## Status values

ABI 2 preserves ABI-v1 values `0..7`:

| Value | Meaning |
|---:|---|
| 0 | `OK` |
| 1 | `ERROR` |
| 2 | `UNSUPPORTED` |
| 3 | `LIMIT` |
| 4 | `INVALID_ARGUMENT` |
| 5 | `ABI_MISMATCH` |
| 6 | `DUPLICATE` |
| 7 | `BUSY` |

ABI 2 appends distinct values for cancellation, deadline expiry, stale or
conflicting revisions, not-found objects, invalid lifecycle state, verification
failure, denied capability, and I/O failure. Limit exhaustion is not a timeout,
and neither implies semantic rejection.

## Handles

In-process public objects use incomplete C types and explicit retain/release or
destroy operations. Serialized and cross-process handles are 64-bit
generation-checked IDs:

```text
zero          invalid
low 32 bits   bounded slot
high 32 bits  nonzero generation
```

Hosts validate kind, owner context, generation, lifecycle state, thread
confinement, and operation permission on every use. Slots may be reused only
after generation advancement; generation wrap retires the slot. Raw addresses
are never serialized.

## Strings, bytes, spans, and source locations

Text and bytes are length-aware. An empty view may have a null pointer; a
nonempty view may not. UTF-8 requirements are stated per field and never inferred
from NUL termination. Owned buffers identify the allocator/context that releases
them.

Source spans contain source identity plus zero-based half-open byte offsets.
Line/column values are optional derived display data, not alternate identity.
Spans are validated against the exact immutable source revision.

## Allocation

The host allocator table contains version/size, context, aligned allocate,
reallocate, and release functions. Allocation size and alignment are bounded and
validated. A component releases memory through the same table that created it.
Allocator failure returns `LIMIT` unless a more specific allocation status is
introduced; it never publishes a partial object.

Descriptor strings and direct arrays are deep-copied by registries. Callback
code and explicitly documented userdata remain provider-owned until unregistered.

## Diagnostics

ABI 2 diagnostics are versioned records with:

- stable machine code;
- note, warning, or error severity;
- message as a length-aware UTF-8 view;
- primary source span and bounded related spans;
- optional category/phase/provider;
- candidate fix-its clearly marked as proposals.

Diagnostic sinks return status so exhaustion or cancellation is observable.
ABI-v1 void sinks are adapted with the existing limits. A diagnostic cannot
upgrade a candidate to verified or certify a repair.

## Cancellation and budgets

Long operations receive borrowed cancellation and budget handles. Polling is
cooperative unless the execution profile can enforce termination. Budgets have
separate counters for work, bytes, allocations, diagnostics, recursion/depth,
IPC, and artifacts. Wall-clock deadlines are separate from deterministic work
budgets.

## Threading and reentrancy

- Registries, mutable sessions, builders, and transactions are thread-confined
  unless explicitly marked otherwise.
- Immutable snapshots may be retained and read concurrently only through APIs
  declaring that property.
- A callback cannot destroy its owner, mutate the active registry transaction,
  recursively invoke itself, or retain callback-scoped views.
- Lifecycle hooks are serialized by the host and cannot unwind through C.

## Candidate publication

Builders produce candidate snapshots. Publication requires:

1. exact source revision match;
2. structural and schema verification;
3. canonical type/effect/ownership/safety checks appropriate to the level;
4. backend/legalization checks when applicable;
5. atomic commit to a new immutable revision.

Transformation validation and structural verification are separate results.
The API must not label a transformation behavior-preserving without the stated
evidence.

## Independent wire versions

Plugin messages, manifests, AST/HIR/MIR snapshots, contracts, catalogues, and
runtime modules carry their own version and bounded decoder. SDK ABI
compatibility does not imply wire-schema compatibility.
