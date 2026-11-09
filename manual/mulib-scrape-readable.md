# mulib-scrape-readable: Remove markup and collapse HTML whitespace into readable text (basic subset).

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Remove markup and collapse HTML whitespace into readable text (basic subset). The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SCRAPE_READABLE` in exactly one C11 translation unit. Use `#include "mulib-scrape-readable.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape-readable.c -o /tmp/test-scrape-readable
/tmp/test-scrape-readable
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape-readable.c -o /tmp/example-scrape-readable
```

## API

```c
/* Extract plain text, drop tags and scripts/styles, collapse whitespace; entities are not decoded. */
size_t mulib_scrape_readable(const char *html,char *out,size_t cap);
```

## Safety and ownership

Drops complete tags, comments and script/style bodies, ignoring ASCII tag case and respecting quotes. Collapses ASCII HTML whitespace and inserts a space at tag boundaries, including inline tags; entities remain encoded. Unterminated comments/raw-text bodies stop extraction, while non-tag `<` text is retained. This is a lexical subset, not visual readability/CSS/HTML5 parsing. Returns bytes actually written (excluding NUL), truncating to capacity minus one with no trailing separator. Zero also means empty output or invalid pointers/capacity. A nonzero-capacity valid output is always terminated. Exact in-place extraction is supported; other overlap is not. No allocation occurs.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape-readable.h`](../include/mulib-scrape-readable.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE_READABLE` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h) |
| Complete example | [`examples/modules/scrape-readable.c`](../examples/modules/scrape-readable.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape-readable.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
