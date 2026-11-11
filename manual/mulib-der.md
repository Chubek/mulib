# der — bounded ASN.1 DER decoding

**Header:** `include/mulib-der.h` · **Switch:** `MULIB_IMPL_DER`

No allocation or module dependencies. A `mulib_der_cursor_t` borrows input and
tracks an offset. `mulib_der_next` returns 1 for a complete TLV, 0 at exact end,
and -1 for malformed/truncated input. Errors and end preserve cursor and output.
`mulib_der_take(cursor,expected_tag,out)` returns 1 only for that tag, preserving
both outputs on mismatch or error. Tags include ASN.1 class/constructed bits.

`mulib_der_value_t` exposes borrowed encoded and content spans.
`mulib_der_contents(value)` creates a cursor over content; a NULL value produces
an empty cursor. Parent cursors do not advance when children are read.

The decoder rejects indefinite lengths, nonminimal long-form lengths, truncated
headers/content, EOC tags and high-tag-number encodings. It is a low-level TLV
decoder, not a generic ASN.1 schema validator: callers check expected primitive
types, container exhaustion, ordering and per-type canonical encoding.

`mulib_der_unsigned(value,&bytes,&length)` accepts only a nonnegative canonical
INTEGER and borrows its unsigned magnitude, stripping a necessary sign-padding
zero. Zero itself has one byte. Negative values and unnecessary sign padding
fail. `mulib_der_u32` additionally requires a magnitude at most four bytes.
Both preserve outputs on failure.

[`examples/modules/der.c`](../examples/modules/der.c) decodes a SEQUENCE containing
INTEGER 42. `tests/test-tls-crypto.c` includes malformed-length/integer vectors;
[X.509](mulib-x509.md) applies certificate-specific schema checks above this layer.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-der.h`](../include/mulib-der.h) |
| Implementation switch | `MULIB_IMPL_DER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/der.c`](../examples/modules/der.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/der.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
