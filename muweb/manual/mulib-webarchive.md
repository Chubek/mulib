# mulib-webarchive: Write WARC-like minimal record headers into a bounded caller buffer.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Write WARC-like minimal record headers into a bounded caller buffer. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBARCHIVE` in exactly one C11 translation unit. Use `#include "mulib-webarchive.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webarchive.c -o /tmp/test-webarchive
/tmp/test-webarchive
cc -std=c11 -Wall -Wextra -Iinclude examples/webarchive.c -o /tmp/example-webarchive
```

## API

```c
size_t mulib_webarchive_record_header(const char *url,size_t body_bytes,char *out,size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
