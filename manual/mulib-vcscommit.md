# vcscommit — canonical Git commit construction

[Manuals](README.md) · [Library index](index.md)

Implementation switches: `MULIB_IMPL_GIT`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_VCSREPO`, `MULIB_IMPL_VCSCOMMIT`.
See [`examples/modules/vcscommit.c`](../examples/modules/vcscommit.c).

Fill `mulib_vcscommit_t` with a tree ID, up to 64 parent IDs, author and
committer identities, and a byte-counted message (up to 16 MiB). Identities
contain name/email strings, a signed Unix timestamp and timezone offset in
minutes (-1439..1439). Names/emails must be nonempty, at most 1024 bytes, with
no control bytes or angle brackets and no leading/trailing spaces. Encoding
is deterministic and preserves message bytes, including NUL and final-newline
presence. It emits `tree`, `parent`, `author`, `committer`, blank line, message.

`encode` returns 1 and transfers malloc-owned, NUL-terminated storage to the
caller (length excludes the additional NUL). Free it with `free`. Input strings
and arrays are borrowed during the call. On failure return 0 and outputs remain
unchanged. Do not pass output variables already owning allocations.

`store` additionally checks that the tree exists with type tree and all parents
exist with type commit, then inserts the encoded object into a vcsrepo. It
does not update any reference; use `mulib_vcsrepo_ref_update` explicitly.
Signatures, arbitrary commit headers, parsing, merge computation and tree
construction are outside this focused builder. `vcscomitt` is the compatibility
spelling. Tests: `tests/test_vcs_formats.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vcscommit.h`](../include/mulib-vcscommit.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vcscommit.h
 * @brief Canonical Git commit construction and repository insertion.
 * Define MULIB_IMPL_VCSCOMMIT plus GIT, VCSREPO and STRBUF implementations.
 */
#ifndef MULIB_VCSCOMMIT_H
#define MULIB_VCSCOMMIT_H
#include "mulib-strbuf.h"
#include "mulib-vcsrepo.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcscommit_identity
  {
    const char *name, *email;
    int64_t timestamp;
    int timezone_minutes;
  } mulib_vcscommit_identity_t;
  typedef struct mulib_vcscommit
  {
    mulib_git_oid_t tree;
    const mulib_git_oid_t *parents;
    size_t parent_count;
    mulib_vcscommit_identity_t author, committer;
    const void *message;
    size_t message_length;
  } mulib_vcscommit_t;
  /** @brief Serialize to malloc-owned NUL-terminated bytes (NUL excluded from
   * length). <=64 parents, <=16 MiB message, identity fields <=1024 bytes.
   * No signatures/extra headers. 1 success/0 failure; outputs unchanged on
   * failure. Inputs are borrowed. Output must not already own storage. */
  int mulib_vcscommit_encode (const mulib_vcscommit_t *, char **bytes,
                              size_t *length);
  /** @brief Validate referenced tree/parent object types then insert commit.
   * 1 success/0 invalid, missing dependency, limit or allocation error. */
  int mulib_vcscommit_store (mulib_vcsrepo_t *, const mulib_vcscommit_t *,
                             mulib_git_oid_t *);
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
| Header | [`include/mulib-vcscommit.h`](../include/mulib-vcscommit.h) |
| Implementation switch | `MULIB_IMPL_VCSCOMMIT` |
| Repository-local includes | [mulib-strbuf](mulib-strbuf.md), [mulib-vcsrepo](mulib-vcsrepo.md) |
| Complete example | [`examples/modules/vcscommit.c`](../examples/modules/vcscommit.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vcscommit.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
