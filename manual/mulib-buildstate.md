# MuLIB BuildState

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-buildstate.h`  
**Switches:** `MULIB_IMPL_SHA`, `MULIB_IMPL_BUILDSTATE`

BuildState provides deterministic action identities, content provenance,
environment allow-list checks and timestamp/content invalidation decisions. It is
based on the fingerprint, provenance, hermetic, incremental, invalidation and
reproduction kernels in `.sot/buildsys`.

## Canonical action identity

`mulib_buildstate_action_t` is an inert borrowed description containing argv,
cwd, explicit environment entries, logical inputs/outputs, timeout and priority.
Validation rejects missing command arguments, empty required names, duplicate
environment names, duplicate paths and input/output overlap.

`mulib_buildstate_action_fingerprint` encodes all fields with typed,
length-delimited canonical framing and returns lowercase SHA-256 hex. Environment
entries and input/output paths are sorted before hashing, while argv order is
preserved. This avoids process-specific pointer or structure representations.

`mulib_buildstate_provenance_fingerprint` adds sorted input-content digests
through an injected callback. Missing files have an explicit representation
distinct from empty files. `mulib_buildstate_file_fingerprint` is the optional
C-stdio SHA-256 adapter with a byte limit.

Fingerprints identify declared data; they do not prove that an arbitrary native
tool avoided undeclared state. `mulib_buildstate_hermetic` only verifies that
declared environment names are allow-listed. Enforcing filesystem, network,
clock and process isolation belongs to an execution sandbox.

## Invalidation

`mulib_buildstate_is_dirty` obtains metadata through an application probe:

- `TIMESTAMP` compares the newest input with the oldest output.
- `CONTENT` requires equal input/output counts and pairwise regular-file digests.
- `ALWAYS` is always dirty.

No outputs, missing paths or unavailable required digests are dirty. Probe errors
are propagated. This separation keeps host filesystem interpretation outside the
core policy.

## Example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude \
  examples/modules/buildstate.c -o /tmp/opencode/buildstate
/tmp/opencode/buildstate
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-buildstate.h`](../include/mulib-buildstate.h) |
| Implementation switch | `MULIB_IMPL_BUILDSTATE` |
| Repository-local includes | [mulib-sha](mulib-sha.md) |
| Complete example | [`examples/modules/buildstate.c`](../examples/modules/buildstate.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/buildstate.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
