# On1x-native SDK subset

This directory is the On1x-authored, bounded value layer for the SDK 2 native
reference profile. It models statuses, trivial `Result` values, immutable
program snapshots, generation IDs, bounded byte/text views, diagnostics,
extension metadata, capabilities, lifecycle events, cancellation/work budgets,
and staged candidates.

The profile deliberately exposes no pointer, address, allocator, callback, raw
AST/HIR/MIR object, foreign resource, executable resolver, or capability grant.
`_Trusted.on1x` is a private raw scalar declaration set for the statically
linked host-approved bridge in `on1x/sdk/native/reference_extension`. It is not
general FFI admission and must not be linked to an arbitrary implementation.

All handles are host-owned `generation32-slot32` IDs. Views are borrowed until
their snapshot is closed. A candidate is untrusted staging data until the host
validates its schema/source revision and atomically publishes it. Diagnostic
sink and cleanup failures are returned as statuses. Work limits are
deterministic counters, not deadlines.

The current compiler restricts nominal payloads to bounded trivially copied
values. General resources, callback retention, ABI 2 host tables, isolated
workers, structured compiler IR, and safe foreign plugin APIs remain
unsupported.
