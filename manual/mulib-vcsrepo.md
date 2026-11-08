# vcsrepo — bounded object storage

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vcsrepo.h`](../include/mulib-vcsrepo.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vcsrepo.h
 * @brief Bounded owned Git-object repository and compare-and-swap references.
 * Define MULIB_IMPL_VCSREPO and MULIB_IMPL_GIT once. In-memory, not a .git
 * directory implementation; no worktree/index or concurrent synchronization.
 */
#ifndef MULIB_VCSREPO_H
#define MULIB_VCSREPO_H
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcsrepo_object
  {
    mulib_git_oid_t id;
    char type[7];
    unsigned char *data;
    size_t length;
  } mulib_vcsrepo_object_t;
  typedef struct mulib_vcsrepo_ref
  {
    char name[256];
    mulib_git_oid_t id;
  } mulib_vcsrepo_ref_t;
  typedef struct mulib_vcsrepo
  {
    mulib_vcsrepo_object_t *objects;
    mulib_vcsrepo_ref_t *refs;
    size_t count, ref_count, max_objects, max_refs, bytes, max_bytes;
  } mulib_vcsrepo_t;
  /** @brief Initialize empty storage, 1 success/0 invalid or allocation
   * failure. */
  int mulib_vcsrepo_init (mulib_vcsrepo_t *, size_t max_objects,
                          size_t max_refs, size_t max_bytes);
  /** @brief Copy/deduplicate an object. 1 success, 0 invalid/allocation/limit,
   * -1 SHA-1 collision (same id but different content/type). */
  int mulib_vcsrepo_put (mulib_vcsrepo_t *, const char *type, const void *,
                         size_t, mulib_git_oid_t *);
  /** @brief Borrow immutable object until repository free; NULL if absent. */
  const mulib_vcsrepo_object_t *mulib_vcsrepo_get (const mulib_vcsrepo_t *,
                                                   const mulib_git_oid_t *);
  /** @brief Read a named reference; 1 found, 0 absent/invalid. */
  int mulib_vcsrepo_ref_get (const mulib_vcsrepo_t *, const char *,
                             mulib_git_oid_t *);
  /** @brief Set refs/... name to an existing object. expected=NULL requires
   * absence; otherwise requires equal existing id. 1 updated, 0 invalid/limit,
   * -1 compare failed. Ref names follow the documented strict Git subset. */
  int mulib_vcsrepo_ref_update (mulib_vcsrepo_t *, const char *,
                                const mulib_git_oid_t *expected,
                                const mulib_git_oid_t *next);
  /** @brief Free owned objects and references, zero state; NULL-safe. */
  void mulib_vcsrepo_free (mulib_vcsrepo_t *);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vcsrepo.h`](../include/mulib-vcsrepo.h) |
| Implementation switch | `MULIB_IMPL_VCSREPO` |
| Repository-local includes | [mulib-git](mulib-git.md) |
| Complete example | [`examples/modules/vcsrepo.c`](../examples/modules/vcsrepo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vcsrepo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
