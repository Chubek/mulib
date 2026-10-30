# objfilelibsearch

Checked short-name Unix ar member lookup. `find` validates all member header
trailers, decimal lengths, name-field termination, payload/padding bounds before
publishing the first matching borrowed span. Returns 0 found, 1 absent, -1
invalid/malformed/unsupported; output unchanged on failure. Input must outlive
the returned span and must not be modified during use.

No filesystem library search, symbol index, thin archives, special members or
long-name extensions. Pair with object parser to inspect member contents.
Switches: `MULIB_IMPL_OBJFILELIBSEARCH`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/objfilelibsearch.c` also uses the ar writer.
