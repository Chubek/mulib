# Trusted linked ABI-2 MIR analyzer

This is one explicit `on1xc` profile, not a replacement for the ABI-1 SDK
hooks or a dynamic ABI-2 loader. Configure one trusted provider source with
`-DON1X_LINKED_SDK_EXTENSION_V2=/absolute/path/to/provider.c` and build
`on1xc`. To exercise the reference fixture, set the path to
`on1x/sdk/tests/linked_extension_v2.c` and put a single
`fn main() -> i64 { return 19 * 3; }` in `fixture.on1x`:

```sh
on1xc --sdk-analyze linked-v2:inspect fixture.on1x
```

The provider defines `on1x_sdk_linked_extension_v2()` and returns its
ABI-2 descriptor. Register a named `On1xSDK_AnalyzerV2` facet from
`register_all` after requesting the `mir.analyze` revision 1 capability.
This profile grants only that capability; registration uses the usual
registry transaction, dependencies, negotiation, initialization and rollback.
Only one linked ABI-2 extension can be configured for this profile.
`--sdk-analyze NAME` without the prefix remains the ABI-1 compatibility
route. A linked-v2 analyzer cannot be combined with `--plugin`, used as
an emitter, or selected by searching `ON1X_PLUGIN_PATH`.

For source input, the compiler first loads, rewrites, checks and lowers the
program. With `--ir-input` the compiler instead decodes and checks canonical
MIR (no claim about the input's source). Both routes go through the existing
SDK MIR admission and verification machinery before the extension entry point
is called. The callback borrows exactly one immutable, revision-bound snapshot
and may use the `On1xSDK-MIR.h` traversal APIs. It must release derived owned
objects before it returns. No mutable compiler state or candidate submission
authority is provided.

The sink validates and copies at most 128 ABI-2 diagnostics, each with a
nonempty UTF-8 message of at most 4096 bytes and no NUL byte. This slice admits
only unlocated records: no source locations, related spans, fix-its or
provider-supplied provenance. The CLI labels the records as SDK MIR output;
it cannot infer source spans from the provider's MIR findings. Failed sink
submissions, non-OK callback returns and shutdown failures discard the staged
diagnostics, with no ABI-1 or DSO retry. Successful error-severity diagnostics
are published and cause exit status 1.

The host bounds MIR admission and returned data. This **trusted in-process**
profile cannot enforce a CPU, memory or wall-clock budget on arbitrary native
provider code. It is not a sandbox and should only link source that the host
authorizes to run with compiler process privileges. C/C++ static initialization
must be inert; the named hook controls SDK entry points, not the native
process loader. ABI-2 dynamic/worker dispatch, PPX-v2, transforms, artifact
emission and writeback are not part of this profile.
