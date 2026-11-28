# Web text codec (bounded C11)

[`mulib-web-textcodec.h`](../kernels/mulib-web-textcodec.h) is an offline
static-inline kernel. It needs no implementation switch, external MuLIB
archive, or platform provider. It uses the existing
[`mulib-utf16.h`](../kernels/mulib-utf16.h) encoder and the staging byte helpers
(`muba_span`, `muba_buf`, `muba_free`). It does not change the independent
[`web-encoding`](../kernels/mulib-web-encoding.h) helper.

## Streaming UTF-8

Initialize a `mulib_web_textcodec_decoder` with
`mulib_web_textcodec_decoder_init(&decoder, limit, fatal, ignore_bom)`.
`limit` is the cumulative emitted-byte budget for that decoder, not a
per-chunk or input-byte limit. Zero permits no output. Count input bytes
separately if a host needs to bound the number or size of its calls.

Call `mulib_web_textcodec_decoder_feed(&decoder, input, finish, &output)` with
each borrowed span; mark the final call with nonzero `finish`. Chunks may be
empty (`{NULL, 0}`), including the final call. The decoder retains only a
partial scalar and its continuation state, never an input pointer. It returns
UTF-8 (not UTF-16 code units) and preserves embedded NUL bytes. One leading
U+FEFF is stripped by default, even if split across calls. `ignore_bom != 0`
preserves it; subsequent U+FEFF scalars are always preserved.

Nonfatal mode replaces a malformed prefix with one U+FFFD and reprocesses
the first disallowed continuation byte. An incomplete prefix at EOF also
produces one U+FFFD; without EOF it remains pending. Fatal mode reports an
error instead. Overlong encodings, surrogates and values above U+10FFFF are
not accepted as scalars. For the supported UTF-8 algorithm, see the [WHATWG
UTF-8 decoder](https://encoding.spec.whatwg.org/#utf-8-decoder).

| Return value | Meaning |
| --- | --- |
| `MULIB_WEB_TEXTCODEC_OK` | This call consumed the whole input and replaced the output with this call's emitted bytes (which may be empty). |
| `MULIB_WEB_TEXTCODEC_INVALID` | Invalid argument or input after successful EOF. |
| `MULIB_WEB_TEXTCODEC_MALFORMED` | Malformed UTF-8 detected in fatal mode. |
| `MULIB_WEB_TEXTCODEC_LIMIT` | The next scalar or replacement would exceed the remaining emitted-byte budget. |
| `MULIB_WEB_TEXTCODEC_NO_MEMORY` | The output allocation failed. |

Compare explicitly with `MULIB_WEB_TEXTCODEC_OK`; errors may be negative.
Failure leaves the decoder and output unchanged, so a caller can retry with
different input or reset the decoder. A successful EOF seals the decoder until
reinitialized. These are C kernel semantics, not JavaScript `TextDecoder`
object or exception semantics.

Start `output` as `{0}` and release it with `muba_free`. Consume each output
before the next successful call, which frees and replaces its allocation; it
does not append or provide a terminator. Successful nonempty calls allocate
exactly the emitted byte count after a validation/sizing pass. During
replacement the old output and new allocation can coexist. Input may view the
old output bytes but must remain valid and unchanged throughout the call.
Do not forge decoder/buffer fields, shallow-copy owned output, or access an
instance concurrently without external synchronization.

## Legacy one-shot API

`mulib_web_textcodec_encode(units, count, &output)` takes native `uint16_t`
code units, combines paired surrogates, and replaces unpaired surrogates.
`mulib_web_textcodec_decode(input, &output)` performs replacement decoding, has
no configured output limit, and keeps the original BOM-preserving default.
It now groups valid incomplete UTF-8 prefixes instead of replacing them
byte-by-byte. Both return 1 on success and 0 on failure, with the same buffer
ownership rules.

This kernel does not implement encoding labels, Windows-1252,
UTF-16 byte-stream decoding, JS bindings, or `TextDecoderStream` scheduling.

## Example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
    examples/modules/web-textcodec.c -o web-textcodec
./web-textcodec
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
    tests/test_web_textcodec.c -o test-web-textcodec
./test-web-textcodec
```

The [example](../examples/modules/web-textcodec.c) prints `€` followed by a
line feed. The [focused suite](../tests/test_web_textcodec.c) checks all
partitions of short valid/malformed fixtures, split BOM and EOF, exact
allocation and budget enforcement, retry after fatal/limit/allocation failure,
and one-shot encoding and ownership. Its CMake target is
`mulib_test_web_textcodec`.
