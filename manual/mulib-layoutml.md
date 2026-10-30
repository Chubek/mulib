# layoutml

Strict XML profile:

```xml
<layout direction="row" gap="4"><item weight="1"/><item weight="3"/></layout>
```

Direction is row/column, optional gap defaults zero. Numbers are unsigned decimal
integers 0..1000000; <=4096 empty item elements, each with exactly weight. Unknown
attributes/elements, nesting, comments and non-whitespace text are rejected.
XML syntax uses the existing bounded XML parser, with no DTD/external entities.
`parse` computes rectangles for a caller-supplied parent; 1 success, 0 error,
optional static diagnostic. Output is malloc-owned; caller frees it. Empty input
layout succeeds with NULL array/count zero; other outputs unchanged on failure.

Switches: `MULIB_IMPL_LAYOUTML`, `MULIB_IMPL_LAYOUT`, `MULIB_IMPL_XML`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`. Example: `examples/modules/layoutml.c`.
