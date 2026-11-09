# mulib-multipart: Multipart/form-data boundary and field formatting for simple text fields.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Multipart/form-data boundary and field formatting for simple text fields. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_MULTIPART` in exactly one C11 translation unit. Use `#include "mulib-multipart.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-multipart.c -o /tmp/test-multipart
/tmp/test-multipart
cc -std=c11 -Wall -Wextra -Iinclude examples/multipart.c -o /tmp/example-multipart
```

## API

```c
/* Format single text field into caller buffer; returns byte length or 0 on error. */
size_t mulib_multipart_field(const char *boundary,const char *name,const char *value,char *out,size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
