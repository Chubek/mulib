# MuLIB kernel registration and interoperability

`include/mulib-*.h` is the source inventory; `MANIFEST.yaml` records each
independent kernel's header, implementation kind and dependencies. The contract
tools consume that registry, and `contracts-exotract/kernel-auto.json` supplies
automatic KernelBind discovery. All 2,049 current public headers are registered:
1,547 implementation-switch modules and 502 bounded modules. The three formerly
auxiliary headers are now explicit entries: `container-core` and `qvm` are
static-inline helpers; `lprowse` contains static data and is discovery-only.

## Choose the existing integration path

| Path | Entry point | Result |
|---|---|---|
| Contract inventory | `python3 on1x/tools/on1x-contracts.py lint` | Checks source-backed inventory and existing semantic annotations |
| Implementation planning | `python3 on1x/tools/on1x-contracts.py build-plan MODULE` | Canonical dependency and implementation-switch plan |
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

This is a declaration-inspection request. Registration alone does not provide
an On1x representation for every C signature. Callable profiles require their
documented ABI and admission evidence; unsupported calls receive diagnostics.
Static-inline definitions retain internal linkage unless explicitly selected
for supported raw forwarding wrappers.

## Keep registration current

After adding a kernel, synchronize the manifest with the repository's inventory
workflow, then regenerate and check automatic discovery:

```sh
python3 tools/extend-kernelbind.py
python3 tools/extend-kernelbind.py --check
python3 on1x/tools/on1x-contracts.py lint
```

Implementation planning deduplicates switches into one native implementation
translation unit and includes registered companion sources. Consumers should use
the generated archive and declaration headers rather than independently defining
the same switches. Static-inline and static-data dependencies are included in a
switched kernel's native plan without receiving implementation switches or
external symbols. Direct unsupported archive requests for those modules are
rejected; declaration inspection remains available.
