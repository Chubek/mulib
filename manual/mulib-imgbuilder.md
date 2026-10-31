# mulib-imgbuilder

## NAME

Deterministic, transactional image builds from rootfs and process recipes.

## INTEGRATION

Define `MULIB_IMPL_IMGBUILDER` and transitive `imgbuildspec`/`rootfsbuilder`
switches. The dependency table and complete runnable TU helper are in
[container-stack](container-stack.md). Example: `examples/modules/imgbuilder.c`.

## BUILD PIPELINE

`build(spec, optional_base, out, report)` validates the recipe, initializes a
root with its node/per-file limits, merges an optional borrowed base, applies
all filesystem steps transactionally, exports canonical USTAR and packs the
process configuration/image envelope. Neither base nor recipe is changed.

The base's aggregate bytes must fit the recipe result budget before copying.
Every recipe step is checked against the same result budget. Existing base
files can be replaced by FILE steps. COPY and REMOVE see the intermediate root.

Output paths are sorted bytewise, independent of insertion order. USTAR uses
zero UID/GID/mtime and 0755 directories/0644 files. Image bytes are reproducible
for equal logical files and process strings; argv/env ordering is preserved.
Only regular files/directories in the existing USTAR writer's path profile are
accepted. Long unrepresentable paths fail rather than truncate.

## ARCHIVE-ONLY EXPORT

`archive(root, limit, out)` sorts a temporary shallow node table, computes an
overflow-checked 512-byte padded archive bound, and exports without modifying
root. The temporary table owns only its table storage; file/path referents stay
borrowed. The final archive is copied into a CT-owned blob.

## REPORTS AND OWNERSHIP

Initialize output to `{0}`. It is replaced only on complete success and may
contain an earlier image. report is initialized even on failure; failed_step
is SIZE_MAX unless a recipe operation failed. Successful reports contain file
and explicit-directory counts, payload bytes, archive bytes and final image bytes.
detail stores underlying filesystem errors where available.

Peak memory includes base/recipe references, a temporary root, transactional
recipe clone, archive buffers and final image. The archive preflight prevents
building beyond max_image_bytes. No external archiver or compiler is spawned.

## RESULTS

Uses `MULIB_CT_*`. Recipe validation, allocation, quota and serialization errors
propagate; unsupported USTAR export shapes produce EFORMAT. Rootfs cloning/merging
and archive-buffer allocation failures also report ENOMEM; overflow and quota
failures report ELIMIT. Full import is in [image2container](mulib-image2container.md).
Tests verify deterministic output,
empty roots, failure-preserved outputs and codec round-trips.
