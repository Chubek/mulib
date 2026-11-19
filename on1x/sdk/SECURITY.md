# SDK security and trust boundaries

This is a design and test contract, not a security-audit claim.

## Authority

Only the host grants capabilities, publishes verified revisions, admits foreign
interfaces, and publishes artifacts. Manifests and contracts are untrusted
claims until independently validated. Requested capabilities and granted
capabilities are distinct values.

## Execution profiles

| Profile | Trust | Enforcement |
|---|---|---|
| static linked | trusted | process authority; explicit build selection |
| in-process DSO | trusted | explicit host authorization; no sandbox claim |
| native worker | untrusted plugin code | namespace/seccomp/resource/IPC policy or fail closed |
| On1x native | depends on selected profile | same policy as equivalent native artifact |
| WASM | unsupported until engine adapter exists | no metadata-only sandbox claim |

Isolation setup completes before plugin constructors, resolvers, or entry points.
Failure never falls back to in-process execution.

## Data boundaries

- Control frames are versioned, length-delimited, bounded, and reject unknown
  operations, duplicate fields, trailing bytes, stale handles, and invalid state.
- Data-plane snapshots are immutable and carry schema, revision, size, digest,
  and provenance. Shared-memory regions are sealed where supported.
- No live compiler pointer, mutable registry, allocator pointer, file descriptor,
  or host path is serialized to a plugin.
- Output remains untrusted bytes until the host validates and atomically publishes
  the expected artifact type.

## Required limits

Hosts enforce or explicitly report unsupported enforcement for CPU, address
space, allocations, deterministic work, wall-clock deadline, IPC frames,
diagnostics, artifact bytes, open handles, children/threads, and filesystem or
network authority.

## Required negative tests

Tests cover malformed and oversized manifests/frames/snapshots, unauthorized
capabilities, duplicate or cyclic registration, stale generations, constructor
side effects, isolation failure, cancellation, worker crash, protocol mismatch,
shared-memory mutation attempts, output truncation, and artifact publication
failure.

## Foreign code

Foreign declaration extraction is not safety admission. A plugin cannot
self-certify contracts or reviewers. Native AOT modules loaded by `On1xRT` are
not sandboxed merely because the runtime SDK loaded them.
