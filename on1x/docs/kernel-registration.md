# MuLIB kernel registration and interoperability

`include/mulib-*.h` is the source inventory. `MANIFEST.yaml` records each
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
