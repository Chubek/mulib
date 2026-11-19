# External SDK 2 backend example

[`external_backend.c`](external_backend.c) is a strict C11 provider for
serialized, host-verified MIR schema 1. It declares one deterministic,
target-independent `MIR_TEXT` capability and emits a receipt prefix followed by
the canonical bytes. Registration copies all descriptor data; callback code and
userdata would remain provider-owned.

The example never parses unverified source, resolves a command, writes a file,
or publishes an artifact. The host selects it explicitly or by compatible
capabilities and gives it a bounded staging sink. The
`on1x.sdk.external_backend` test invokes it through the public API and requests
a two-run reproducibility comparison.

It is deliberately linked as a test fixture rather than discovered by name:
extension discovery and capability authorization belong to the core/plugin
subsystem.
