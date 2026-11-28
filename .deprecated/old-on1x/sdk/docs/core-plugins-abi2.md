# On1x SDK ABI-2 core and plugin facilities

ABI 2 is a separate descriptor family. `ON1X_SDK_ABI_VERSION` remains `1` so
existing ABI-v1 sources and layouts do not change; new descriptors use
`ON1X_SDK_ABI_VERSION_V2` or `ON1X_SDK_ABI_CURRENT`.

## Core ownership

- Every extensible ABI-2 structure starts with `abi_version` and `struct_size`.
- Contexts copy allocator and callback tables. Owned byte buffers retain the
  context when their allocator table is context-owned.
- Handles encode a nonzero generation in the high 32 bits and a bounded,
  nonzero slot in the low 32 bits. Handle operations validate context, kind,
  generation, current state, and the context's handle thread.
- Registries and catalogues are thread-confined. Registry snapshots and
  catalogue plans own their metadata and are immutable.
- Diagnostics and strings are length-aware. Source identity is the
  `(source_id, revision, half-open byte range)` tuple; display positions do not
  substitute for it.
- Cancellation deadlines use absolute monotonic nanoseconds. Deterministic
  budget counters are independent from wall-clock deadlines.

## Registration

`on1x_sdk_registry_register_batch_v2` validates and copies all descriptors
before callbacks run. It then:

1. resolves semantic-version dependencies;
2. orders dependencies first and otherwise uses the stable registration key;
3. intersects requests with host grants;
4. stages registration callbacks;
5. initializes in dependency order;
6. commits once, or invokes shutdown compensation in reverse order and removes
   every staged registry entry.

The host table passed to each callback is synchronous and capability-scoped.
Its context token and borrowed views must not be retained. A manifest request
never becomes a grant.

ABI-v1 registration remains available through
`on1x_sdk_registry_register_v1` and the original
`on1x_sdk_registry_register` spelling. Unknown v1 callback statuses map to
`ON1X_SDK_ERROR`.

## Catalogues

The catalogue adapter deep-copies declarative entries and invokes MuLIB's exact
bounded CDD planner over at most 64 entries. Callers can inspect entries only
through checked accessors; no MuLIB array or executable callback is exposed.

`ON1X_SDK_LIMIT` from resolution means the search stopped. A returned plan may
contain a feasible incumbent with `found=1, optimal=0`; it is not an
optimality certificate.

## Plugin trust and execution

Manifest discovery parses caller-provided bytes and sorts inert records. It
does not map or execute an image. Authorization separately verifies SHA-256
and invokes an explicit host trust callback.

The Linux native compatibility worker uses protocol 2:

- bounded framed messages form the control plane;
- MIR input is a digest-checked read-only shared mapping;
- output is a distinct staging mapping and remains untrusted until the host
  freezes it, checks its generation/owner/size/digest, and parses the expected
  result;
- namespace, private root, credentials, resource limits, and seccomp setup
  complete before `dlopen`, constructors, IFUNC resolvers, or entry points;
- setup failure terminates activation, with no in-process fallback.

The current System V mapping transport provides local read-only mappings and
digest verification, but not kernel sealing. Accordingly,
`sealed_shared_memory_available` is false. WASM remains explicitly unsupported
until an engine and enforcement adapter exist.

See `sdk/protocol/plugin-control-v2.md` for wire details.
