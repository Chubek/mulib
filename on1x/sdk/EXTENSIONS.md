# SDK 2 extension model

## Manifest

An extension manifest is declarative, versioned, bounded, and canonical. It
records name/version, SDK ABI range, dependencies, provided facets, compiler
phases, requested capabilities, execution profile, resource requests, artifact
fingerprints, and trust/review claims.

Discovery parses manifests without loading executable code. Names and versions
do not authorize binaries, filesystem paths, commands, backends, or contracts.

## Activation

Activation is explicit:

1. discover deterministic candidates;
2. select an unambiguous identity;
3. validate manifest, fingerprint, ABI and dependencies;
4. compute requested/granted capability intersection;
5. select an allowed execution profile;
6. register in one transaction;
7. initialize or roll back with reverse compensation.

Built-in commands and mandatory compiler phases cannot be shadowed. Namespaced
extension commands remain distinguishable from host commands.

## Facets

Facets declare input representation/schema, required validation state,
supported phases and targets, effects, budgets, and output kind. An analyzer
publishes revision-bound facts and diagnostics. A transformer submits a
candidate transaction. A backend produces a staged artifact. A frontend
produces a source/AST/HIR candidate and declares its semantic subset.

## Isolated invocation

The host sends immutable snapshots through the data plane and a small operation
request through the control plane. The worker receives only granted
capabilities. Cancellation and lifecycle events are control messages. Every
response names the request, source revision, operation version, and status.

## Compatibility

V1 analyzers and backends retain their serialized-MIR compatibility path.
PPX-v1 remains available for its admitted syntax subset. Neither path is
misrepresented as structured SDK 2 access.
