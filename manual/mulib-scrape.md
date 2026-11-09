# mulib-scrape: Simple query-free HTML text extraction orchestrator.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Simple query-free HTML text extraction orchestrator. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SCRAPE` and `MULIB_IMPL_SCRAPE_READABLE` in exactly one C11 translation unit. Include `mulib-scrape.h`; implementations are never enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape.c -o /tmp/test-scrape
/tmp/test-scrape
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape.c -o /tmp/example-scrape
```

## API

```c
/* Extract readable text from HTML through a caller-owned buffer. */
size_t mulib_scrape_text(const char *html,char *out,size_t cap);
```

## Safety and ownership

`mulib_scrape_text` delegates to [scrape-readable](mulib-scrape-readable.md), sharing its lexical profile: complete tags/comments/script/style bodies are removed, ASCII HTML whitespace collapses and tag boundaries separate text. Returns bytes actually written excluding NUL, truncating to capacity minus one. Zero also means empty/invalid. Exact in-place operation is supported, other overlap is not. Entities remain encoded. No fetching, DOM/CSS querying, JavaScript or browser orchestration is performed.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape.h`](../include/mulib-scrape.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE` |
| Repository-local includes | [mulib-scrape-readable](mulib-scrape-readable.md) |
| Complete example | [`examples/modules/scrape.c`](../examples/modules/scrape.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
