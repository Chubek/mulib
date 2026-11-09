# mulib-browsersnapshot: Owned immutable byte snapshot with FNV-1a change fingerprint.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned immutable byte snapshot with FNV-1a change fingerprint. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERSNAPSHOT` in exactly one C11 translation unit. Use `#include "mulib-browsersnapshot.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browsersnapshot.c -o /tmp/test-browsersnapshot
/tmp/test-browsersnapshot
cc -std=c11 -Wall -Wextra -Iinclude examples/browsersnapshot.c -o /tmp/example-browsersnapshot
```

## API

```c
typedef struct mulib_browsersnapshot {unsigned char *data;size_t size;uint64_t hash;} mulib_browsersnapshot_t;
int mulib_browsersnapshot_create(mulib_browsersnapshot_t *s,const void *data,size_t size);
void mulib_browsersnapshot_free(mulib_browsersnapshot_t *s);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
