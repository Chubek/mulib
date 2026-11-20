# Sandbox kernels: build and inspect, not import

On1x can inspect declarations and build a C11 archive for the eight MuLIB
sandbox kernels. Their canonical C exports are **not callable KernelBind
imports**. The separate [explicit raw adapter](kernel-framed.md) for
`mulib.ipc_framed` exposes local byte transport, not the sandbox API.
`--kernel-review-candidate` cannot create a review subject for these kernels.
The build does not install filters, change namespaces, launch workers, or
attest platform enforcement.

| `import kernel` spelling (canonical declaration inspection) | Archive module |
|---|---|
| `mulib.sandbox_profile` | `sandbox-profile` |
| `mulib.seccomp` | `seccomp` |
| `mulib.landlock` | `landlock` |
| `mulib.namespace` | `namespace` |
| `mulib.cgroup2` | `cgroup2` |
| `mulib.ipc_framed` | `ipc-framed` |
| `mulib.capbroker` | `capbroker` |
| `mulib.sandbox_process` | `sandbox-process` |

`sandbox-process` selects `sandbox-profile`, `seccomp`, `landlock`, `namespace`,
`cgroup2` and `ipc-framed`; `capbroker` is separate. Building the two together
deduplicates the shared profile. The MANIFEST planner and Exotract `emit-source`
put all selected switches in **one** implementation TU; the C ABI is not
redeclared in On1x. The profile fixes C11, `_POSIX_C_SOURCE=200809L` and
`_GNU_SOURCE=1`, and permits only the documented optimization, debug and
sanitizer codegen flags. All `MULIB_IMPL_*` switches are off during discovery.
The sandbox archive is built with strict C warnings.

## Reproduce the evidence

From the MuLIB root, using the configured checkout and fresh output paths:

```sh
sdk=build/sandbox-kernelbind-sdk
cmake -S on1x -B "$sdk" -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOML=ON \
  -DON1X_WITH_EXOML_EXTRACT=ON -DON1X_WITH_EXOTRACT=ON \
  -DON1X_WITH_EXOTRACT_LUA=ON
cmake --build "$sdk" --target on1xc exotract --parallel 4
ctest --test-dir "$sdk" -R '^on1x.kernel_sandbox_sdk$' --output-on-failure

exo="$sdk/libstone-moonstone/exotract/exotract"
resource="$(clang -print-resource-dir)"
python3 on1x/tools/on1x-build.py --inspect-kernels --no-prelude \
  --on1xc "$sdk/on1xc" --exotract "$exo" --cc gcc --resource-dir "$resource" \
  on1x/tests/fixtures/kernel_sandbox.on1x -o "$sdk/sandbox-inspection.json"
python3 on1x/tools/on1x-exotract.py build sandbox-process capbroker \
  --exotract "$exo" --compiler gcc --resource-dir "$resource" \
  --output "$sdk/sandbox-archive"
```

The common [ExoLib recipe](../contracts-exotract/kernel-discovery-v1.lua) invokes
LibStone's `exolib.new`, `Preprocess`, `Parse` and `Bind`. The inspection report
and per-module `*.sandbox-evidence.json` include the original Exotract On1x
metadata, complete target-specific ExoML CDecl v2 graph and SDK-normalized ExoML
sidecar. The CDecl pins the public header, dependency headers and manuals,
sidecar, ExoLib script, catalog and MANIFEST, including their SHA-256 hashes.
The archive adds `symbols.json`, a build plan, and a lock with native compiler,
target, input hashes and artifact hashes. The archive builds every module in
the closure, and verifies all declared public symbols against linker exports.
`not_representable`, `callable:false` and `safe_export:false` are intentional;
none of these files is a safe binding certificate.

A Lua-enabled Exotract linked to ExoML extraction, libclang, and compatible
Clang resource headers are required. No SDK detection success is inferred from
a MANIFEST entry. Linux x86-64 GNU is the measured target; Linux AArch64 GNU is
eligible for extraction but requires its own ABI/build and host-enforcement
validation. Other profiles are unsupported. See [measured results](../VERIFICATION.md#sandbox-kernel-discovery-and-archive).

## Review obligations and trusted code

The [eight ExoML v2 sidecars](../contracts-exoml/sandbox/) cover all 37 public
functions. They describe bounded lifetimes/effects where expressible and keep
opaque authority and unknown effects conservative. Each sidecar is bound to a
header revision and marked **annotated / unreviewed**. Re-evaluate the
obligations before re-pinning after any API or implementation change. Actual
CDecl extraction, sidecar identity and SDK structural validation are checking
inputs; none of them proves the claims below.

| Module | Required checks before a safe projection |
|---|---|
| `sandbox-profile` | Target-qualified by-value record ABI, record layout and width, checked version/ranges, static result-string borrow. Valid configuration is not proof of enforcement. |
| `seccomp` | Live checked-element spans and exclusive policy state, stable borrowed descriptor identity, architecture/policy validation and irreversible effects confined to trusted bootstrap. A failed installation is not evidence of an unchanged process. |
| `landlock` | Owned rule descriptor identities lent through live immutable rule spans, exact ABI and rights validation, irreversible bootstrap transition. No descriptor revocation, automatic fallback or full mediation claim. |
| `namespace` | Child-only state transition with distinct mount/PID semantics; failure may leave setup partially applied. Do not infer enforcement from partially written output. |
| `cgroup2` | Borrow an administrator-delegated directory, bounded immutable name/limits, unique group and owned descriptors; stable supervisor-owned process identity. A failed destroy retains responsibility; live processes borrowing the group must be cleaned up first. |
| `ipc-framed` | Unique stream owner, exclusive/non-escaping framing state, bounded nonoverlapping byte spans, output publication only on success, absolute monotonic deadline, no retry after poison, explicit EOF/timeout/cleanup semantics. |
| `capbroker` | Host-only exact capability policy and payload validation; approved string/table borrow tied to host lifetime. Nested callback and context borrows live through audit and finish; no reentry/concurrency/unwind across C. Definite failure aborts; uncertain commit never gets an automatic rollback or retry. |
| `sandbox-process` | Approve and pin the trusted bootstrap and its dependencies; do not load or execute untrusted plugin code before successful entry. Borrow rule descriptors and dedicated group, track unique child/IPC owner, do not publish partial data, and preserve child cleanup ownership on failed launch/stop/reap. No concurrent or competing reaper; readiness does not alone establish enforcement. |

Where ExoML cannot encode a branch-dependent resource transfer or a callback
inside a C record, the obligation remains in source-review notes and the
symbol stays unadmitted. There is no generated pointer/callback adapter,
unsafe cast, checked callback-free iterator, or On1x worker supervisor in this
slice. Keep native callback and enforcement glue in the trusted host boundary.
On1x owns decisions about routing, manifest and permissions at the application
layer; C kernels provide mechanisms and do not grant application authority.
Production use also needs separate host enforcement runs (including delegated
cgroups, Landlock and AArch64) and targeted independent security review.
Application worker and data-plane orchestration remain separate host concerns.
