# Building and composing MuLIB libraries

[Manuals](README.md) · [Library index](index.md)

## One source file

Define implementation switches **before** including headers. A switch emits
definitions; merely including a header normally exposes declarations.

```c
#define MULIB_IMPL_STACK
#include "mulib-stack.h"

int main(void)
{
    mulib_stack_t *stack = mulib_stack_create();
    int value = 42, result = 0;
    if (!stack)
        return 1;
    if (!mulib_stack_set_stride(stack, sizeof value)
        || !mulib_stack_push(stack, &value)
        || !mulib_stack_pop(stack, &result)) {
        mulib_stack_destroy(stack);
        return 1;
    }
    mulib_stack_destroy(stack);
    return result == 42 ? 0 : 1;
}
```

Save as `example.c`, then run from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude example.c -o example
./example
```

Each library manual links its repository example and a Linux build command.
Those commands use `-UNDEBUG` because examples often perform operations inside
`assert`; compiling them with assertions disabled would skip those operations.
They include `-pthread -lm -ldl` for a consistent Linux baseline, even when a
particular module needs fewer libraries. Other platforms require their native
equivalents and must satisfy the module's platform requirements.

## Dependencies and multiple source files

Implementation switches are application-owned. Defining a module's switch does
not automatically enable its dependencies. Use the module's documented recipe
or linked example; direct header includes can also expose optional APIs whose
implementations are not needed for every operation.

For example, put the JSON implementation in `mulib_impl.c`:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include "mulib-json.h"
```

Other files include `mulib-json.h` without these definitions. Compile and link
the implementation once with the application:

```sh
cc -std=c11 -Iinclude -c mulib_impl.c -o mulib_impl.o
cc -std=c11 -Iinclude app.c mulib_impl.o -lm -o app
```

When combining recipes, deduplicate shared implementation switches and place
all required definitions before the first MuLIB include. Avoid distributing
implementation switches through global build flags. Static-inline support
headers explicitly document that they need no implementation switch.

### C++ consumers

Compile implementation files as C11, then link their objects with the C++
application. Public C-linkage declarations do not imply that every implementation
is valid C++ source:

```sh
cc -std=c11 -Iinclude -c mulib_impl.c -o mulib_impl.o
c++ -std=c++17 -Iinclude app.cpp mulib_impl.o -lm -o app
```

### CMake consumers

The `mulib` interface target supplies include paths, C11 requirements, and
threads. It does not emit implementation definitions. For a JSON application:

```cmake
add_subdirectory(path/to/mulib mulib-build)
add_executable(app app.c mulib_impl.c)
target_link_libraries(app PRIVATE mulib)
if(NOT WIN32)
  target_link_libraries(app PRIVATE m)
endif()
```

Add `${CMAKE_DL_LIBS}` for modules that require dynamic loading. Optional
separately compiled layers have their own build instructions, including
[Virtio](mulib-virtio-os.md) and [Shiraz](mulib-shiraz.md).

## Diagnosing build problems

| Symptom | Check |
| --- | --- |
| Undefined `mulib_*` symbol | Implementation switch and dependency implementations are present in a linked C file. |
| Duplicate `mulib_*` definitions | A shared implementation was emitted in multiple translation units. |
| Undefined math, thread or loader symbol | Required platform libraries appear in the link command, after objects where applicable. |
| Missing compatibility header | OCI/imgtracker forwarding headers still use sibling files in `.deprecated/`; preserve that directory when copying them. |
| C++ implementation compile error | Compile implementation as C11 and use C linkage from C++. |
| Example does no work in Release | Keep assertions enabled (`-UNDEBUG` for GCC/Clang). |

## Verification

```sh
python3 tools/manuals.py --check
python3 tools/manuals.py --compile-examples
```

The second command compiles and links examples in temporary directories without
running programs or accessing devices. For execution and broader checks, see
the individual stack guides, [hardening](hardening.md), and the repository
[testing instructions](../README.md#extensive-examples-and-test-coverage).
