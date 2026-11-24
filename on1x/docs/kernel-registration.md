# MuLIB kernel registration and interoperability

## Host-only workspace harness

The four workspace/process kernels are included in the registry and raw archive
planner. They have no callable On1x or MuLL profile: their C interfaces use
owned buffers, pointers, mutable process or workspace state, and callbacks.

Trusted Python hosts can use `on1x/tools/on1x_harness.py` to dispatch
data-only requests from a harness client. Construct one `Supervisor` per
session with host-selected workspace roots, optional named `Tool` recipes, and
explicit call, byte and deadline budgets. Pass only bounded, decoded requests
of the forms `{"op":"read","root":"name","path":"relative/file"}` or
`{"op":"run","tool":"name"}`. Other operations, including networking and
workspace edits, are denied. This helper is not installed into the language
runtime and does not admit raw foreign calls.

The Linux profile reads regular files from pinned descriptors without
following symlinks. Roots and their backing filesystem must be host approved:
this is not protection against external hardlinks, hostile mounts or stalled
filesystem I/O. The optional tool launcher requires a trusted Bubblewrap
installation and a dedicated immutable image, without secrets or host
sockets. Caller-supplied paths, executable flags and environment overrides
are not accepted. The named executable gets only its configured environment
and a read-only image in private namespaces including network and PID; there
is no unsandboxed fallback. Only use trusted tools and nonsecret environment
values (setup values are launcher arguments). This profile provides call,
output and wall-clock budgets, not cgroup CPU, memory or process quotas.

Run `python3 -B on1x/tests/test_harness_supervisor.py cc` to exercise workspace
denials and budgets and, when available, real isolated tool launch.

## Registration

`mulib.quic` and `mulib.http3` support declaration inspection with both
`import kernel` and `import kernel[dynlib]`. Inspection attaches their
source-pinned, unreviewed ExoML contracts. The contract, Exotract archive, and
MuLIB source planners use explicit live-client recipes: `quic` selects native
TLS/UDP and `http3` adds the HTTP/3 GET client. Combined plans place each
implementation switch once in a single C11 translation unit. Direct calls from
On1x (including `unsafe import kernel`) remain unsupported: these APIs require
buffer and client ownership, borrowed-view lifetimes, and separate host network
authorization. The raw archive is for trusted C consumers, not an admission.

`kernels/mulib-*.h` is the source inventory. `MANIFEST.yaml` records each
kernel's header, implementation kind and dependencies. Contract, archive and
source-bundle tools read that shared registry. The generated
`on1x/contracts-exotract/kernel-auto.json` supplies declaration discovery for
both plain `import kernel` (static linkage) and `import kernel[dynlib]`.
There is no second per-kernel list to maintain for those import spellings.
Comment-only reservations are recorded in the manifest but not exported as
kernel imports. The contract and archive inventories list existing placeholders
separately as `reserved_headers`.

## Choose the existing integration path

| Path | Entry point | Result |
|---|---|---|
| Static import inventory | `python3 on1x/tools/on1x-exotract.py list-kernels` | Exact `mulib.<alias>` spellings, discovery routes and callable profile requirements |
| Contract inventory | `python3 on1x/tools/on1x-contracts.py lint` | Checks source-backed inventory and existing semantic annotations |
| Contract implementation planning | `python3 on1x/tools/on1x-contracts.py build-plan MODULE` | Dependency and implementation-switch plan for configured ExoML core profiles |
| Archive inventory | `python3 on1x/tools/on1x-exotract.py list` | Registered modules, build recipes and contract availability |
| Archive planning | `python3 on1x/tools/on1x-exotract.py plan MODULE` | Selected native build closure, including discovery-only classifications |
| Source and fixture bundles | `python3 on1x/tools/on1x-mulib.py plan MODULE` | Canonical sources and registered fixture dependencies for bundling |
| Native source requests | `on1xc --kernel-imports program.on1x` | Source imports, aliases and locations |
| Declaration inspection | `on1x-build.py --inspect-kernels ...` | Real target-qualified Exotract/CDecl metadata |
| Native calls | `on1x-build.py --kernel-bind ...` | Executable code for the documented admitted profiles |
| Raw inline wrappers | `on1x-exotract.py wrap-inline MODULE:C_SYMBOL ...` | Explicit ABI-checked C11 forwarding archive for supported signatures |
| Existing library adapters | Ordinary `std.mulib` imports and build helper | The implemented standard-library adapters |

For extraction, archive and wrapper commands, supply the compiler, extractor and
output options shown in the [Exotract manual](../contracts-exotract/README.md).
For native source imports and inspection, follow [KernelBind](kernel-bind.md).
The [contract manual](contracts.md) describes pinned semantic sidecars and their
separate review requirements.

Source names normalize hyphens to underscores, `+` to `_plus_`, and prefix a
leading digit with `k_`. For example:

```on1x
import kernel mulib.media_timebase as Time;
```

This spelling selects static linkage for a callable profile. It also works for
declaration inspection even if that kernel has no callable profile.
Registration alone does not provide an On1x representation for every C
signature. Callable profiles require their documented ABI and admission
evidence; unsupported calls receive diagnostics. Static-inline definitions
retain internal linkage unless explicitly selected for supported raw forwarding
wrappers.

In `list-kernels`, `source_status: eligible` means the source name resolves;
it does not mean the declarations were extracted or the kernel compiled.
`static_compile` reports the On1x call gates, not the independent C archive
recipe. The checked unsafe profiles cover `clamp` and `ipc-framed`; the
reviewed profiles cover `clamp`, `histprice` and `priceopt`. The registry
contains no review grant.

## Keep registration current

Run `python3 tools/install_hooks.py` once to enable the checked-in pre-commit
hook in this clone. It refreshes the shared manifest and generated discovery
catalog from staged inputs; the contract, archive and source-bundle tools see
the manifest update without separate registrations. The
[contributor workflow](../../CONTRIBUTING.md) explains what to stage and which
build/test steps remain separate.

To update registration manually and verify the generated catalog:

```sh
python3 tools/sync_inventory.py
python3 tools/extend-kernelbind.py --check
python3 on1x/tools/on1x-contracts.py lint
```

For a direct manifest edit that needs no combined-test fixture updates,
`python3 tools/extend-kernelbind.py` refreshes the discovery catalog alone.

Implementation planning deduplicates switches into one native implementation
translation unit and includes registered companion sources. Consumers should use
the generated archive and declaration headers rather than independently defining
the same switches. Static-inline and static-data dependencies are included in a
switched kernel's native plan without receiving implementation switches or
external symbols. Direct unsupported archive requests for those modules are
rejected; declaration inspection remains available.
