# mulib-imgbuildspec

## NAME

Reusable owned image recipes combining process configuration and filesystem steps.

## INTEGRATION

Define `MULIB_IMPL_IMGBUILDSPEC` and the transitive switches for `imgformat` and
`rootfs-buildspec`; see [container-stack](container-stack.md). Example:
`examples/modules/imgbuildspec.c`.

## CONFIGURATION

`init(spec, id)` creates a process spec, an empty filesystem recipe and these
defaults: 4096 nodes/steps, 16 MiB per file, 64 MiB recipe storage and result file
bytes, 128 MiB image bytes, portable architecture. Process string quotas retain
the container module defaults. Change limits before adding files/arguments.

Use `mulib_container_spec_arg/env/cwd` on `spec.process`. Set UID/GID, read-only
and resource requests there. `spec.architecture` accepts PORTABLE, AMD64, ARM64,
OIX or QVM; this labels the image for backend selection and does not translate
programs to another architecture.

`file(spec, path, bytes, length)` adds a copied FILE step and checks the per-file
quota immediately. `directory(spec, path)` adds a directory step. Advanced
COPY/REMOVE steps use `spec.filesystem` and rootfs-buildspec directly. Missing
parents are created during execution, not when the recipe is recorded.

## VALIDATION AND LIFETIME

`validate` checks static image quotas, architecture, process accounting, step
count and recorded file lengths. It cannot determine whether a later COPY
source exists; that is resolved by the builder against its intermediate root.

`clone` deep-copies process strings, recipe paths and file data into an
uninitialized output. Outputs are unchanged on failure. `free` releases both
subspecifications and resets all fields. Do not shallow-copy an initialized spec.
The builder borrows recipes only during a build, so a recipe can produce multiple
independent images and can be freed immediately after building.

## RESULTS

All operations return `MULIB_CT_*`. Invalid specifications produce EINVAL;
per-file or aggregate recorded quotas produce ELIMIT; allocation failures produce
ENOMEM. Runtime resource enforcement belongs to the selected backend.

See [imgbuilder](mulib-imgbuilder.md) for deterministic build reports and
[imgformat](mulib-imgformat.md) for serialized fields and compatibility.
