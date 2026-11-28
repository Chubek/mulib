# Contributing to MuLIB

Every new module must have a single self-contained `kernels/mulib-<name>.h`, a public header guard,
`extern "C"` declarations, a `MULIB_IMPL_<NAME>` implementation switch, a dedicated Doxygen comment
for the file and functions, one detailed `manual/mulib-<name>.md`, and a runnable
`examples/modules/<name>.c` example. Register it in `MANIFEST.yaml`.

The manifest's `module_count` includes reserved filenames;
`implemented_module_count` counts working modules. A `status: reserved` entry is
only for an empty/comment-only future header, must not be a dependency, and must
not be described as an implemented API. Implementing it requires the same
manual, example, switch, and test wiring as any other module.

Implementation switches must be defined in exactly one source translation unit. All public
objects use explicit ownership rules and `mulib_` symbol prefixes. Avoid globals that
cause symbol collisions, document failures and allocation limits, use `size_t` for
capacities, and check multiplication for overflow before allocating. Generic containers
currently copy fixed-stride bytes and are not automatic destructors of nested objects.

Before changing an interface, run `python3 tools/check.py`, the CMake/CTest suite,
strict GCC and Clang compilation if available, and ASan/UBSan functional tests.
Avoid copying third-party sources without recording licensing and provenance.

## Composition and scope

Keep kernel and platform documentation independent of downstream application
identities, deployment plans, and release gates. Document reusable mechanisms
by domain and supported profile. Application policy belongs outside generic
kernel APIs; discovery, test execution, and independent safety approval remain
distinct. Retain honest historical results with their original scope, failures,
and exclusions when reorganizing documentation.

Reuse existing MuLIB storage, parsing, encoding, and document models when they
fit. Include dependency headers normally and list transitive implementation
switches in the manual and standalone example. Never silently define another
module's implementation switch: the application owns implementation placement.

For format and protocol modules, document the exact supported grammar, rejected
features, resource limits, and platform assumptions. A focused API must be fully
implemented; the module name is not a claim of full standards conformance.
Unsupported input should produce a diagnostic rather than silently disappear.
Keep the default library free of third-party dependencies.

Add success, malformed-input, empty-input, boundary, ownership, allocation-failure
(where injectable), and roundtrip tests. New focused suites live in
`tests/test_*.c` and are discovered by CMake and `tools/check.py`. The checker
also compiles headers independently with their transitive implementation
dependencies and validates the manifest/manual/example inventory. CMake's
`MULIB_SANITIZERS` option and the checker's `--sanitizers` flag enable ASan/UBSan.

After adding headers, `python3 tools/sync_inventory.py` registers new modules and
updates combined-test inclusion without replacing existing manifest descriptions.
It also promotes implemented reservations and refreshes dependency/switch fields;
invalid input is rejected before writing inventory files.
The same command refreshes On1x's generated KernelBind discovery catalog.

Enable automatic registration for this clone with `python3 tools/install_hooks.py`.
When you stage a new `kernels/mulib-*.h`, also stage its manual and any required
manifest metadata. The pre-commit hook uses staged inputs and stages changes
only to `MANIFEST.yaml` and `on1x/contracts-exotract/kernel-auto.json`. It stops
rather than overwriting unstaged edits to either output. Use `git add` followed
by an ordinary `git commit`, not `git commit --only <paths>`, when registration
needs to update the index. If you already have a pre-commit hook, the installer
leaves it in place; run `.githooks/pre-commit` from the repository root in your
existing hook chain.

The hook is metadata-only. Run the full inventory sync for combined-test
wiring, for promotions of existing reserved headers, and when consuming
`MANIFEST.yaml.additions` or `MANIFESTS.yaml.additions`.
A new module without an implementation switch
needs an explicit manifest entry with `implementation: static-inline` or
`implementation: static-data` and `status: bounded`, unless it is a
comment-only reservation.

For direct edits to `MANIFEST.yaml`, run `python3 tools/extend-kernelbind.py`
(or `make extend-kernelbind`). Verification checks for drift; registration
does not create an ABI adapter or grant permission to call a kernel.
Then regenerate dependency documentation with `python3 tools/stratify.py`.
Neither command implements APIs or replaces handwritten examples/manuals.

Public-header changes also require refreshing the MuLL native bridge with
`python3 armory/scripts/generate_mulib.py --include kernels` (Python libclang
bindings are required for generation). The normal verification suite runs
`python3 armory/scripts/check_mulib_inventory.py --include kernels` without
libclang to catch missing registrations and stale generated artifacts. Add
MuLL integration fixtures under `armory/tests/mull/mulib/` when introducing
new callable APIs; Armory CTest exercises them through both available hosts.

Follow the [manual authoring guide](manual/authoring.md) for ownership, return
values, supported profiles, and runnable examples. After changing a header or
manual, run `python3 tools/manuals.py --write` to refresh marked source references
and the library index, then `python3 tools/manuals.py --check` to verify them and
local links. The latter is also part of `tools/check.py`. Behavioral prose remains
handwritten; the generator does not infer contracts from signatures.

For shared storage or parser changes, also run `tests/test_hardening.c` (moving
allocation/failure injection) and `tests/test_hostile_inputs.c`. The optional
Clang `MULIB_BUILD_FUZZER` target enables bounded mutation fuzzing; see
`manual/hardening.md` for commands and verification limits. Do not replace
documented focused APIs with claims of full standards or security compliance.
