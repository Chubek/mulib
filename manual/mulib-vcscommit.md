# vcscommit — canonical Git commit construction

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
