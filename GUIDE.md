# Using MuLIB

[Overview](README.md) · [Installation](INSTALL.md) · [Library index](manual/index.md)

This guide walks through module selection, implementation placement, and
ownership using small C programs. Commands assume you are in the repository
root with a C11 compiler; see [INSTALL.md](INSTALL.md) for build-system setup.

## Find a module

1. Browse the [module guide](manual/modules.md) by task or search the
   [library index](manual/index.md) by name.
2. Read the module's manual for supported syntax, platforms, return values,
   ownership, and limits.
3. Open its linked `examples/modules/` source for a complete implementation
   recipe. Follow any shared setup header included by that example.
4. Use the public header for exact types, signatures, and configuration macros.

[`MANIFEST.yaml`](MANIFEST.yaml) records the inventory and dependencies;
[dependency strata](manual/strata.md) describe how modules fit together.
The include graph can contain optional APIs, so use the documented runnable
recipe when deciding which implementations an application needs.

## Understand the implementation model

An ordinary include exposes declarations. Defining the module's implementation
switch before inclusion also emits its function definitions:

```c
#define MULIB_IMPL_VEC
#include <mulib-vec.h>
```

For each module used by your program:

- Emit the implementation in **exactly one translation unit** (a compiled source
  file and its includes).
- Include the header without the switch in other source files.
- Define the required dependency switches before the first MuLIB include in the
  implementation file. Dependency implementations are not enabled implicitly.
- Combine recipes by deduplicating shared implementations. Avoid setting
  implementation switches globally through compiler flags or public headers.

Some support headers contain static-inline helpers and need no implementation
switch; their manuals describe that model. Separate compiled layers such as
Shiraz and Virtio have their own linking instructions in [INSTALL.md](INSTALL.md).

## Work with a byte container

The [README example](README.md#quick-start) creates a vector of integers with
`mulib_vec_init`, copies a value with `mulib_vec_push`, reads it with
`mulib_vec_at`, and releases storage with `mulib_vec_free`.

Important details from the [vector contract](manual/mulib-vec.md):

- The stride is the element size in bytes. A vector of `int` uses `sizeof(int)`.
- Push copies bytes. A vector of pointers copies the pointers, not their targets.
- Push/reserve return `1` on success and `0` on failure; failed growth preserves
  existing elements.
- `mulib_vec_at` returns a borrowed pointer or `NULL` for an invalid index.
  Growth can invalidate borrowed pointers, so reacquire them after mutation.
- Clear and free do not destroy resources referenced by stored elements.

[`vec`](manual/mulib-vec.md) is a generic container;
[`vector`](manual/mulib-vector.md) provides numerical vector algebra on top of it.

## Use MuLIB in multiple source files

The JSON module demonstrates a composed API. Put its implementation and the
documented dependency implementations in **`mulib_impl.c`**:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include <mulib-json.h>
```

Put the consumer in **`app.c`**, without implementation switches:

```c
#include <mulib-json.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char source[] = "{\"greeting\":\"hello\",\"items\":[1,2,3]}";
    mulib_json_error_t error;
    mulib_json_t *root = mulib_json_parse(source, sizeof source - 1, &error);
    if (!root) {
        fprintf(stderr, "JSON error at byte %zu: %s\n", error.offset,
                error.message ? error.message : "parse failed");
        return 1;
    }

    const mulib_json_t *items = mulib_json_get(root, "items");
    if (!items || mulib_json_typeof(items) != MULIB_JSON_ARRAY) {
        fputs("expected an items array\n", stderr);
        mulib_json_destroy(root);
        return 1;
    }
    printf("items: %zu\n", mulib_json_size(items));

    char *text = mulib_json_stringify(root, 0);
    if (!text) {
        mulib_json_destroy(root);
        return 1;
    }
    puts(text);
    free(text);
    mulib_json_destroy(root);
    return 0;
}
```

Compile the implementation once and link it with the consumer:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c mulib_impl.c -o mulib_impl.o
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude app.c mulib_impl.o -lm -o app
./app
```

Expected output:

```text
items: 3
{"greeting":"hello","items":[1,2,3]}
```

`root` owns the parsed tree. `items` is a borrowed child and is released as part
of that tree, not independently. The serialized `text` is a separate
malloc-owned allocation released with `free`.

JSON strings can contain embedded NUL bytes; use the length-returning accessors
when working with arbitrary string values. Numbers use `double`. See the
[JSON manual](manual/mulib-json.md) for parsing limits and mutation contracts.

## Use MuLIB from C++

Compile implementation files as C11 and include declarations from C++.
Public C-linkage declarations do not imply that every implementation compiles
as C++ source. The `app.c` example above can also serve as `app.cpp`:

```sh
cp app.c app.cpp
cc -std=c11 -Iinclude -c mulib_impl.c -o mulib_impl.o
c++ -std=c++17 -Iinclude app.cpp mulib_impl.o -lm -o app-cpp
./app-cpp
```

For CMake consumers, see the [integration recipe](INSTALL.md#integrate-with-cmake).

## Follow ownership and error contracts

MuLIB has **no universal success/error convention**. Depending on the API,
success may be `1`, `0`, a count, or a non-NULL pointer. Read the individual
function contract before interpreting a result.

| Contract | What it means for callers |
| --- | --- |
| Owned object | Release it with its documented destructor or deallocator |
| Borrowed object or view | Keep the owner alive and respect mutation/invalidation rules |
| Ownership transfer | Check whether transfer happens on success, failure, or both |
| Byte container | Manage pointed-to resources yourself; copying bytes does not clone allocations |
| Length-delimited input | Pass the actual byte length and check whether terminators are included |
| Callback/backend | Respect context lifetime, reentrancy, and thread-safety requirements |

For example, JSON `append` and `set` transfer ownership only on success. On
failure the caller still owns the value; successful replacement destroys the old
member. Copying a structure containing owned pointers does not create a second
independent owner.

Configuration macros and limits belong before implementation inclusion. Keep
configuration consistent across translation units wherever it affects public
types or ABI. Thread safety and allocation-failure guarantees are API-specific.

## Compose existing models

Related modules share data structures so applications can reuse parsed or
constructed values:

- **Configuration:** JSON, YAML, TOML, templates, and JSON-RPC share the JSON
  value model. See [`tests/composition.c`](tests/composition.c) for a
  JSON → YAML → TOML → template pipeline.
- **Parsing:** Parsec's PEG/EBNF loaders produce combinator parsers with owned
  ASTs; visitors, listeners, and event streams operate on shared tree structures.
- **Memory:** allocation tracking and memory pools reuse the allocator layer.
- **Markup:** HTML, SVG, and XPath build on shared XML/document facilities.
- **Builds:** artifacts, task plans, and build graphs provide reusable data and
  execution models; individual backends define execution behavior.

[`examples/dogfood.c`](examples/dogfood.c) combines parser-backed JSON and
numerical vector operations. The [module guide](manual/modules.md) and
[extension-layer guide](manual/extension-layers.md) explain further composition
paths and supported profiles.

## Run and adapt the examples

Each module manual includes a build command for its standalone example. For
JSON, run from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude \
    examples/modules/json.c -lm -o json-example
./json-example
```

Successful completion produces no output; assertions check the result. Keep
assertions enabled when compiling repository examples because some operations
are performed inside `assert`. CMake's example targets already do this.

Start with the module's complete example, retain its dependency setup, and
replace its demonstration calls with your application's logic. Check its manual
for required arguments, assets, and platform facilities before running it.

For larger areas, continue with the [binary-format guide](manual/binary-formats.md),
[storage/server guide](manual/storage-server.md),
[container-stack guide](manual/container-stack.md),
[text-stack guide](manual/text-hardening.md), or
[VMM integration manual](manual/mulib-vmm.md).
Build troubleshooting and verification commands are in [INSTALL.md](INSTALL.md).
