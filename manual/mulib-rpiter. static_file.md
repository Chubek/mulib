# Static files

Historical filename retained. Define `MULIB_IMPL_STATIC_FILE`, `VFS`, `MIME`,
`SECURITY`, `HTTP_RESPONSE` and their full closures. Strict absolute VFS paths
are percent-decoded once, stat-checked against a byte limit, read and copied to
an owned HTTP response. No query/index/range/native symlink support. Lookup
failure is INVALID; callers choose public status mapping.
[Example](../examples/modules/rpiter.%20static_file.c), [contracts](storage-server.md).
