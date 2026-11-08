# ASCII Soundex

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PHONETIC_UTILS`.
[Example](../examples/modules/phonetic_utils.c).

`mulib_phonetic_soundex` writes four characters and a terminator to a caller's
five-byte buffer. It retains the uppercased first letter, encodes consonants,
suppresses adjacent repeated codes, pads with zeroes and truncates to length
four. Vowels/Y separate codes; H/W do not. Entire input is validated before
output changes. Require a nonempty NUL-terminated ASCII-letter-only word;
NULL, punctuation, digits and non-ASCII bytes return 0 without output mutation.
No allocations. Soundex is a lossy English-name heuristic, not a pronunciation
model or multilingual phonetic transcription.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-phonetic_utils.h`](../include/mulib-phonetic_utils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-phonetic_utils.h
 * @brief ASCII English Soundex phonetic keys.
 * Define MULIB_IMPL_PHONETIC_UTILS. Not a multilingual pronunciation engine.
 */
#ifndef MULIB_PHONETIC_UTILS_H
#define MULIB_PHONETIC_UTILS_H
#ifdef __cplusplus
extern "C" {
#endif
/** Write a four-character key and NUL. Accept ASCII letters only, nonempty.
 * H/W do not separate equal codes; vowels and Y do. Failure preserves output. */
int mulib_phonetic_soundex(const char *word, char output[5]);
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
| Header | [`include/mulib-phonetic_utils.h`](../include/mulib-phonetic_utils.h) |
| Implementation switch | `MULIB_IMPL_PHONETIC_UTILS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/phonetic_utils.c`](../examples/modules/phonetic_utils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/phonetic_utils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
