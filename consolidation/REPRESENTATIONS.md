# Representation boundaries

| Domain | Boundary for this increment |
|---|---|
| Sparse linear algebra | Preserve the existing `mulib-num-csr` API and ABI. `mulib_num_csr_t` owns mutable offset, column and value arrays; Amb3r's `ma_csr` borrows `size_t` offsets/indices and values. Use checked adapters to preserve ownership, lengths and ordering; a layout cast is not a conversion. |
| Geometry and hardware | Do not alias `ma_point`, `ma_box`, or `ma_hw_context` to a general geometry/HWIL type on layout alone. Record coordinate units, orientation, object identity, direction, and signal width in conversion contracts. |
| Binary data and text | Bytes carry explicit lengths and may contain NUL. A text value is not necessarily a C string; C-string projection needs explicit validation or a copying adapter. Never publish a temporary native pointer as a language value. |
| Media | Rational timestamp units are seconds per tick. Media frame views describe independent plane capacities and byte strides; JoltFX float image views carry float-element strides and linear-light straight alpha. GLX format values describe native-endian integer words, not byte-ordered media samples. Keep packed/planar, encoded/linear, and straight/premultiplied distinctions explicit. |
| Objects and resources | C owners, C views, canonical language places and engine-local handles are distinct concepts. Make publication after successful validation explicit; no implicit widening of trust at a conversion. |

These are adaptation rules, not a claim that all cross-family conversions
are implemented. See each implementation and its regression test for exact
limits.

The [representation catalogue](REPRESENTATIONS.json) describes the first
interoperating types. The implemented CSR copy and read-only borrow share
structural validation, preserve duplicates and explicit zeros, and do not
change units. The borrowed view ceases to be valid when its storage is
modified or destroyed. See [the CSR manual](../manual/mulib-num-csr.md) for
capacity and failure behavior.
