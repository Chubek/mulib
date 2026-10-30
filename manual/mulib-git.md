# git — Git object IDs and packet lines

Define `MULIB_IMPL_GIT` in one implementation translation unit. No dependencies.
See [`examples/modules/git.c`](../examples/modules/git.c).

`mulib_git_sha1` hashes arbitrary bytes, including empty input; `object_id`
hashes Git's `type SP decimal-length NUL content` representation. Supported
types are `blob`, `tree`, `commit`, and `tag`. Object content is opaque: hashing
a tree does not validate its entries. IDs are 20-byte values. `oid_format`
writes 41 bytes including NUL; `oid_parse` requires exactly 40 hexadecimal
characters and accepts either case. SHA-256 repositories are outside this API.
SHA-1 is provided for Git compatibility, not authentication.

`pkt_encode` and `pkt_decode` implement four-hex-digit pkt-line lengths,
including flush, delimiter, and response-end controls. Public kind values are
0=data, 1=flush, 2=delimiter, 3=response-end. Maximum data length is 65516;
`0003` and lengths above 65520 are invalid. Empty data encodes as `0004`.
Decode returns 1 complete, 0 incomplete, -1 invalid; retain bytes on incomplete.
Payload pointers borrow the input. `used` is reset on failure. Encoders require
disjoint input/output storage. There are no allocations, subprocesses, Git
transport negotiation, index operations, or worktree operations.

Tests: `tests/test_vcs_formats.c`, including independent SHA-1 and Git vectors.
