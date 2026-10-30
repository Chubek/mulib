# vcscomitt — compatibility spelling

The canonical API is [`vcscommit`](mulib-vcscommit.md). This header preserves
the requested `vcscomitt` spelling without maintaining a second commit model.

Define `MULIB_IMPL_GIT`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_VCSREPO`,
`MULIB_IMPL_VCSCOMMIT`, and `MULIB_IMPL_VCSCOMITT` in one implementation unit.
Dependency implementation switches are never defined automatically.

`mulib_vcscomitt_t` and `mulib_vcscomitt_identity_t` alias the canonical types.
`mulib_vcscomitt_encode` and `mulib_vcscomitt_store` forward to the corresponding
canonical operations with identical validation, limits, ownership and return
values. Encoding produces malloc-owned bytes; storing copies into the borrowed
repository and leaves reference updates to the caller.

See [`examples/modules/vcscomitt.c`](../examples/modules/vcscomitt.c) and
`tests/test_vcs_formats.c`. Both spellings may coexist in the same program.
