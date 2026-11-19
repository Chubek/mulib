# On1x SDK 2 architecture

## Purpose

SDK 2 is the stable C11 boundary around the existing On1x compiler, its
verified representations, selected toolchain services, and a separate
application runtime API. It extends the canonical compiler; it is not a second
parser, type system, optimizer, backend dispatcher, or foreign-safety authority.

The authoritative path remains:

```text
source candidate
  -> canonical scanner/parser/PPX
  -> canonical elaboration and mandatory semantic checks
  -> checked HIR snapshot
  -> canonical lowering and verifier
  -> verified MIR snapshot, when the language subset is representable
  -> capability-compatible backend
  -> staged artifact publication
```

An extension may inspect an immutable snapshot or construct a candidate. Only
the host can publish a candidate after the canonical checks for that
representation. Structural validity is not evidence of semantic equivalence,
foreign safety, optimization correctness, or source intent.

## Components and dependency direction

1. **Core** — ABI negotiation, host tables, allocators, handles, diagnostics,
   cancellation, budgets, registries, manifests, catalogues, and plugin
   transport.
2. **Compiler/frontend** — reusable sessions, sources, VFS, parsing, AST and
   frontend candidates. It calls the existing frontend.
3. **Types/IR** — immutable semantic snapshots and transactional AST/HIR/MIR
   candidate builders. It adapts existing On1x types and MIR.
4. **Analysis/transformation** — revision-bound analyses and rollback-capable
   pass pipelines. It consumes Types/IR and never publishes directly.
5. **Backends/toolchains** — capability descriptions and artifact production.
   It receives only representations admitted by the host.
6. **Contracts/runtime** — reviewed foreign descriptions, binding generation,
   kernel planning, and the separately versioned `On1xRT` application ABI.
7. **Projects/tooling/bindings** — builds, packages, IDE services, the
   `on1x-sdk` utility, installation, and language wrappers.
8. **On1x-native SDK** — reviewed On1x bindings and native extensions built on
   the preceding public interfaces.

Dependencies point down this list or through public C interfaces. No subsystem
may introduce a competing registry, allocator protocol, type universe, IR
identity, diagnostic schema, serializer, or plugin loader.

## Trust and representation states

Every program or IR handle carries:

- representation kind and independently versioned schema;
- immutable revision identity;
- provenance and target profile;
- validation state: `CANDIDATE`, `CHECKED_HIR`, or `VERIFIED_MIR`;
- owning host context and generation.

External AST/HIR/MIR is always `CANDIDATE`. The following operations are
host-only and are not registrable extension facets:

- mandatory parsing and elaboration;
- type, ownership, borrow, effect, and safety admission;
- foreign ABI and contract admission;
- HIR/MIR verification and candidate publication;
- capability grants;
- artifact publication over an existing output.

Unsupported constructs remain present or cause a located rejection. A lowering
must never silently erase them.

## Lifetime and ownership

- Public objects are opaque handles. A successful create/retain transfers one
  owned reference; `view`, `get`, and callback parameters are borrowed.
- A view is valid only for its documented owner revision and lifetime. Registry
  mutation invalidates registry views and plan indices.
- Memory crossing the ABI is allocated and released through the allocator
  recorded on the owning context. No API implicitly mixes C, C++, plugin, or
  runtime allocators.
- Callbacks are synchronous unless explicitly documented otherwise. They may
  not throw through C, retain borrowed objects, or re-enter a mutating operation
  on the same transaction.
- Registries and mutable builders are thread-confined. Immutable snapshots may
  be shared only when the API marks them shareable.

See [ABI.md](ABI.md) for the concrete C contract.

## Registries, catalogues, and activation

Declarative catalogue records are data. Executable registry entries contain
host-authorized callbacks. Discovery never executes an entry and never grants
its requested capabilities.

Registration is one transaction:

1. validate descriptor headers, strings, limits, manifest, and dependencies;
2. resolve dependencies deterministically (dependency order, then stable
   registration key);
3. negotiate requested versus host-granted capabilities;
4. stage copied descriptors and lifecycle hooks;
5. run initialization under the selected trust profile;
6. commit all staged entries, or compensate in reverse order and roll back.

The SDK may adapt MuLIB CDD planning, but executable callbacks remain an
explicit host allowlist.

## Plugin execution

Profiles are:

1. trusted statically linked;
2. explicitly authorized trusted in-process DSO;
3. isolated native worker;
4. On1x-authored native extension using the same profiles;
5. WASM only when an actual engine and enforcement adapter are available.

The **control plane** carries bounded versioned requests, responses,
diagnostics, cancellation, lifecycle events, and capability decisions.

The **data plane** carries immutable serialized snapshots and artifacts. Large
objects may use sealed shared memory with generation-checked transfer handles.
No live compiler pointer or mutable compiler state crosses a process boundary.
Failure to establish isolation is terminal for that activation; there is no
in-process fallback.

## Revisions and transactions

Analyses are keyed by representation identity, revision, target, options, and
analysis version. Publication of a new revision invalidates affected cached
results. A pass operates on a private transaction, declares preservation, runs
required checks, and commits only against the exact source revision. Stale,
failed, cancelled, or exhausted work leaves the source unchanged.

Artifact generation writes to a host staging sink. A successful, complete
artifact may then be atomically published by the host. Extensions never receive
authority to overwrite an existing output.

## Version families

The following evolve independently:

- compiler extension C ABI: `2`;
- ABI-v1 compatibility adapter: `1`;
- plugin control protocol: `2`;
- AST schema: independently versioned;
- HIR schema: independently versioned;
- MIR schema: existing canonical versions plus SDK envelope version;
- foreign contract schema: independent from the SDK ABI;
- runtime application ABI (`On1xRT`): `1` when introduced.

Version negotiation is explicit. Unsupported major versions fail with
`ON1X_SDK_ABI_MISMATCH` or the corresponding schema/protocol error.

## Integration rule

`on1xc` progressively becomes a consumer of compiler-session, analysis,
pipeline, and backend services, but its mandatory checks remain direct
authoritative host operations. Existing command behavior remains available
while migration is tested. Extensions are never enabled implicitly.
