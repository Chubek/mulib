# mulib-hdl-verilog-lex

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-hdl-verilog-lex.h`  
**Implementation switch:** `MULIB_IMPL_HDL_VERILOG_LEX`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Source-offset tokens for identifiers, escaped identifiers, decimal/based-number-like spans, punctuation and line/block comments. Unterminated block comments return ERROR without reading past the source. No full SystemVerilog preprocessing, string grammar or keyword classification.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_HDL_VERILOG_LEX` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_HDL_VERILOG_LEX
#include "mulib-hdl-verilog-lex.h"
```

## Focused regression test

`tests/test_hdl_verilog_lex.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_hdl_verilog_lex.c -lm -o /tmp/test_hdl_verilog_lex
/tmp/test_hdl_verilog_lex
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hdl-verilog-lex.h`](../include/mulib-hdl-verilog-lex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hdl-verilog-lex.h
 * @brief Source-offset tokens for identifiers, escaped identifiers, decimal/based-number-like
 * spans, punctuation and line/block comments.
 * @details C11 single-header implementation; define MULIB_IMPL_HDL_VERILOG_LEX in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HDL_VERILOG_LEX_H
#define MULIB_HDL_VERILOG_LEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hdl_tokkind {
    MULIB_HDL_EOF = 0,
    MULIB_HDL_ID = 1,
    MULIB_HDL_NUMBER = 2,
    MULIB_HDL_SYMBOL = 3,
    MULIB_HDL_ERROR = 4
} mulib_hdl_tokkind_t;
typedef struct mulib_hdl_token {
    mulib_hdl_tokkind_t kind;
    size_t offset, length;
} mulib_hdl_token_t;
typedef struct mulib_hdl_lexer {
    const char *text;
    size_t length, cursor;
} mulib_hdl_lexer_t;
void mulib_hdl_lexer_init(mulib_hdl_lexer_t *lex, const char *text, size_t len);
mulib_hdl_token_t mulib_hdl_lexer_next(mulib_hdl_lexer_t *lex);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HDL_VERILOG_LEX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hdl-verilog-lex.h`](../include/mulib-hdl-verilog-lex.h) |
| Implementation switch | `MULIB_IMPL_HDL_VERILOG_LEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hdl-verilog-lex.c`](../examples/modules/hdl-verilog-lex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hdl-verilog-lex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
