# KernelBind reviewed scalar profile

The Package C continuation executes **only** `mulib.clamp.add_i64` and
`sub_i64`, both `(i64, i64) -> i64`, on native Linux x86-64 through C11.
It accepts native safe imports conditional on an independently signed review
selected explicitly by the build operator. The shipped ExoML annotation remains
**unreviewed**; no production review or trusted reviewer key is bundled.
This implements an admission mechanism, not an independent audit of the kernel.
The same mechanism now supports selected [numerical profiles](kernel-numerics.md),
including a signed subject for a multi-kernel build. This page documents the
original scalar profile.

```on1x
import kernel mulib.clamp as Clamp;
fn main() {
    println(Clamp.add_i64(40, 2));
}
```

## Review policy and trust boundary

The operator supplies an **Ed25519 public key in DER SubjectPublicKeyInfo
format** using `--kernel-review-key`. Obtain this key from the independent
review authority through a channel controlled outside the application.
There is no source-selected key, key discovery, downloaded policy, default
trust store, signing command, or automatically approved contract.

The authority reviews the canonical C implementation and ExoML claims, exact
target, declarations, build tools, adapter generator, and flags identified in
the review subject. In particular, the two scalar functions must be total for
all `int64_t` argument pairs, nonblocking, resource-free, and free of external
state effects. It returns a JSON file with **exactly** these fields:

```json
{
  "schema": "on1x.kernel-review/v1",
  "reviewer": "accountable reviewer identity",
  "review_reference": "independently accessible audit reference",
  "subject": {"...": "the complete unmodified candidate object"}
}
```

The detached signature is Ed25519 over the **actual bytes** of that file,
not a normalized JSON representation. The subject is compared structurally
with strict JSON scalar types. Duplicate keys, unknown review fields, invalid
key encodings, wrong signatures, missing evidence, and stale input hashes fail.
An operator-controlled key establishes whose judgment is trusted; cryptography
does not establish reviewer competence or prove C semantics. The review
reference and signer identity remain in the published evidence.

Approval covers exactly the two named exports. `Clamp.i64` is rejected with
`ON1X-KERNEL-MEMBER`; the generated interface explains its exclusion. The
signed subject cannot authorize another kernel, target, ABI, tool revision,
contract, or compiler flag set. The importing application's aliases and source
hashes are outside the review subject, but are checked during the build and
recorded in the final lock. Both dependency imports and application calls go
through ordinary namespace resolution, type inference, and C11 lowering.

## Reproducible workflow

Build the SDK as described in [KernelBind](kernel-bind.md). From the repository
root, create a fresh subject:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/kernel/clamp_safe.on1x \
  --kernel-review-candidate --no-prelude \
  --on1xc build/kernelbind-gcc/on1xc \
  --exotract build/kernelbind-gcc/libstone-moonstone/exotract/exotract \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  -o build/clamp-review-subject.json
```

This command runs real declaration-only ExoLib/Exotract extraction and ExoML
validation. It creates an unsigned `on1x.kernel-review-subject/v1` object,
**not** authorization. Send the subject and its pinned inputs to the authority.
Once independently reviewed, build with the returned evidence:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/kernel/clamp_safe.on1x \
  --kernel-bind --no-prelude \
  --kernel-review /trusted/review.json \
  --kernel-review-key /trusted/reviewer.der \
  --kernel-review-signature /trusted/review.sig \
  --on1xc build/kernelbind-gcc/on1xc \
  --exotract build/kernelbind-gcc/libstone-moonstone/exotract/exotract \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  -o build/clamp-admitted
build/clamp-admitted/program
```

**With `--kernel-review`, `-o` names a new directory**, not a binary. The entire
directory is published by Linux atomic no-replace rename only after all gates
pass. Its contents include:

- `program`, compiler-generated `main.c`, `kernel-adapter.c`, and the same
  `kernel-adapter.o` used for the application and executable probe;
- `libmulib.a`, containing the canonical planner's single `mulib.o`, and `mulib.c`;
- full `discovery.json`, `interface.json`, `build-plan.json`;
- original signed review bytes, public key, signature, and `kernel-probe.c`;
- `admission.json` (`on1x.kernel-admission/v1`), with source/tool/runtime hashes,
  target, flags, review evidence, executable-gate result, and artifact hashes.

The adapter includes the actual public header and checks its prototype with
`_Generic` and target size/alignment with `_Static_assert`. Every build executes
1,348 comparisons: generated adapter and direct C calls against a Python
arbitrary-precision oracle, covering both operations, boundary pairs and a
deterministic random corpus. This is executable evidence, not a proof.
The application itself is compiled and linked, not executed during admission.
OpenSSL, Exotract, libclang and Lua are build-time tools only.

Compiler inspection via `--emit-c --kernel-c11 --kernel-stage` emits **unresolved
scalar callers**, not safe admission. Linking those callers with the kernel
archive alone fails: only the gated build driver generates the matching
adapter definitions. This flag cannot create a certificate or bypass the build
driver. Hand-authoring arbitrary C adapters remains outside this trust boundary.

## Limits and tests

This profile has no pointers, spans, records, enums, size conversions, resources,
error adaptation, callbacks, retained state, suspension, or unknown effects.
Effects other than the typed `foreign_call` boundary are rejected. Macro/PPX,
staging, raw externs, task groups, defer, MIR and independent checker modes
remain unavailable for these imports. Legacy scalar extern certificates do not
authorize KernelBind. A reviewed build can also explicitly import the
ABI-verified unsafe clamp surface (even under another alias for the same
kernel). It still requires a lexical `unsafe` block. The bundle records it
separately in `unsafe-interface.json`; admission for the reviewed surface
does not promote the raw alias or duplicate the implementation TU.

Input identity is deterministic for fixed local paths/tools/flags; relocation
and tool updates require fresh review. Debug builds are not promised bit-for-bit
artifact reproducibility. Published evidence is an audit artifact, not a
reusable import certificate or archive cache input. Other targets, cross
compilation and installation relocation of reviewed bundles remain open.

```sh
ctest --test-dir build/kernelbind-gcc \
  -R 'kernel_(imports|review)' --output-on-failure
```

`tests/test_kernel_review_sdk.py` uses **ephemeral test authority keys** to
exercise actual signing/verification, two independent extractions, compiler
calls, one-TU archive linkage, multi-module source execution, direct C
equivalence, and rejection/publication boundaries. Its signatures are not
production reviews and are deleted with the test directory. See
[verification](../VERIFICATION.md) for measured configurations and failures.
