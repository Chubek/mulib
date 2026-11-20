# Consolidation implementation

## Build input provenance

The tracked LibStone gitlink now has a `.gitmodules` entry pointing at
`Chubek/libstone`. Its recorded commit has not changed. `git submodule init
on1x/third_party/libstone` was verified in a separate clone using the staged
index from the starting revision and the new mapping; it registered the path
and URL and showed the expected pin. This local test did not fetch from the
network or change the configured checkout.

On1x's configure-time `on1x-build-info.txt` records the Git source revision
and whether tracked files are locally modified when available, along with the
selected LibStone revision, scanner and toolchains. It now
reports the bounded MIR wire profiles and distinguishes a build with bundled
signed-review KernelBind support from a build with only extraction or without
it. Untracked files and nested checkouts are not covered by the dirty flag.
This file reports build configuration, not reproducibility proof or safety
approval.

The read-only On1x evidence add-on accepts canonical flat manual paths for
both static-inline and implementation-switch modules. This restores indexing
for Amb3r modules that share a family manual. Source path restrictions,
linkage classification and the lack of automatic ABI or foreign admission
remain unchanged.

## CSR and exclusive loans

The Amb3r and numerical CSR implementations share one logical validator.
Checked copy and borrow adapters preserve both public layouts and make the
ownership difference explicit. The executable regressions compose canonical
CSR matvec, Amb3r LU, and a schematic backward-Euler/KCL check. No separate
kernel or foreign admission was added.

On1x now supports `ref_replace` for named exclusive loans, including
supported refcounted and finite move-only types. `Box` uses this to exchange
an inner owner without reallocating the outer owner. C11 emission also retains
a by-value shared-managed parameter before it can be overwritten through an
exclusive loan, so the caller's counted reference is not released early.

## Catalogue, foreign code, compiler and runtime

- MuLL discovery now tracks 1,961 headers and 7,283 function declarations.
  A separate legacy raw policy preserves 5,373 callable entries; the new
  declarations do not add wrappers to that bridge. Graphics and JoltFX
  inventory checks use deterministic tracked inputs.
- Exotract `wrap-inline` emits target-checked, explicitly raw C11 wrappers
  and verifies one archive implementation unit with a separate-TU consumer.
  The tested profile passes flat records by value and scalar pointers, and
  publishes audit evidence rather than a KernelBind admission decision.
- Canonical MIR lowering no longer emits unused latches or joins when
  structured paths terminate. Targeted five-way parity compares direct C11,
  MIR C11, and interpretation, with and without MIR optimization. The
  verifier still rejects unreachable supplied blocks and broken effect chains.
- `on1xc --sdk-analyze linked-v2:HOOK` activates an explicitly linked trusted
  ABI-2 analyzer with a verified MIR snapshot and staged diagnostics. Existing
  ABI-1 calls remain supported; this is not dynamic ABI-2 worker activation.
- On1xRT's Linux/glibc profile snapshots native image bytes into a sealed
  memfd and obtains host approval of their digest before OS loader execution.
  Descriptor checks and lifetime tracking then use the existing embedding ABI.
  Native code and its dependencies are trusted, not sandboxed.

## Application workloads

[Application fixtures](workloads/README.md) add one On1x-authored pricing
program using the signed-review mechanism under an ephemeral test key, and
three C-only oracles for Amb3r, JoltFX and Ulysses. C-only composition never
counts as callable On1x interoperability.

## Workstream evidence

The workers implemented against the same starting revision in detached
worktrees. The IDs below identify their source changes before principal
wiring and regenerated integrated artifacts.

| Workstream | Worker commit | Focused evidence | Not delivered by this slice |
|---|---|---|---|
| Representations | `15e6d77591e9ff0f20fd2ae283c9b8dd82254db0` | 87 Amb3r C checks and C/C++ linkage, strict GCC/Clang and Clang ASan/UBSan; independent CSR/MNA oracles | General EDA, HWIL, and media representation migration |
| Build and inventories | `af38385ae5b86187d3ff3658d1306f3e448b22d4` | Deterministic compiler-backed regeneration, graphics gate, MuLL C++/D conformance | Native-device and all-platform verification |
| Foreign interoperability | `16a1c84f108a23f49ba4aafacdae210bf0c36751` | GCC and Clang ASan/UBSan separate-TU wrapper and ABI-drift tests | General contracts, owner/callback lowering, new On1x admission |
| Ownership | `63f0db76f6c6583a4c3d24287061ff54f5df8ae7` | Strict native lifecycle tests, 3,400 allocations, seven fault points, Clang ASan/UBSan/LSan, forged HIR rejection | General managed text, nested place loans, owner-bearing defer or MIR |
| MIR | `0a7a8392f93c9c642cf0b38d83f0b713660bddbe` | Source and wire parity, optimizer comparison, strict verifier negatives, v1–v3 compatibility | Managed/nominal/effectful MIR |
| SDK | `fdbb2b008e32012f6471d2720f3f8e56a6692cf1` | Both linked ABI profiles and ABI-2-disabled rejection | Native On1x-authored ABI-2 facets, PPX-v2, dynamic workers |
| On1xRT | `9087a6001e24678cd0ed665585c8a0bd9662e627` | Authorized load/call/unload, digest and failure cases, sanitizer run and separate installed consumer | Compiler-generated runtime modules, async and runtime isolation |
| Workloads | `2fbec21e6fe6fc40516b76267d9ca28d1f12b283` | Three-module On1x pricing and C11 reference pipelines under GCC and Clang ASan/UBSan | Three remaining actual On1x ports and production admission |

See [verification](VERIFICATION.md) for the baseline and reruns, and
[remaining gates](REMAINING_WORK.md) for evidence still needed.
