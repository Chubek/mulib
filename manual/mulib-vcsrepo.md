# vcsrepo — bounded object storage

Define `MULIB_IMPL_GIT` and `MULIB_IMPL_VCSREPO` before including the header.
See [`examples/modules/vcsrepo.c`](../examples/modules/vcsrepo.c).

Initialize an uninitialized repository with nonzero object/reference limits
and an explicit total content-byte budget. `put` copies bytes, computes the
Git object ID, and deduplicates equal content. `get` returns an immutable
borrowed object until `free`; object-table storage never relocates. Empty
objects consume an object slot but no content-byte budget. Content is opaque.
Repeated initialization of a live repository is unsupported: free it first.
`free` releases all objects/tables and resets the repository.

`put` returns 1 success, 0 invalid/allocation/limit, -1 for a detected SHA-1
collision with a different stored object. Outputs stay unchanged on failure.
References name existing objects; they do not automatically require commits.
`ref_update` with NULL expected requires an absent reference; otherwise the
existing ID must match expected. It returns 1 updated, -1 compare failure,
0 invalid/limit. Updates are compare-and-swap in API semantics only; callers
must synchronize threads themselves.

Names are at most 255 bytes, start `refs/`, and reject empty/dot-prefixed
components, `.lock` component suffixes, trailing dot/slash, `..`, `@{`, control
bytes, spaces, and Git's reserved punctuation. Symbolic refs, deletion,
reflogs, disk persistence, .git directory loading and worktrees are not exposed.
Tests: `tests/test_vcs_formats.c`.
