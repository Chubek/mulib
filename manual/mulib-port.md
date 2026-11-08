# mulib-port

[Manuals](README.md) · [Library index](index.md)

Platform identification and checked `size_t` arithmetic. ISO C11; no OS or
third-party dependency. Define `MULIB_IMPL_PORT` once.

`MULIB_PORT_WINDOWS`, `MULIB_PORT_POSIX`, and `MULIB_PORT_LINUX` are numeric
0/1 detection macros, not promises of specific operating-system facilities.
`mulib_port_os()` and `mulib_port_cpu()` return borrowed static names;
unknown targets return `"unknown"`. `mulib_port_little_endian()` inspects the
host's byte order.

`add_size`, `mul_size`, and `align_size` return 1 on success and 0 for overflow,
NULL output, or zero alignment. Output is unchanged on failure. Alignment may
be any positive integer, not only a power of two. `secure_zero` erases through
a volatile byte pointer; it is not a substitute for OS memory locking or a
guarantee that copies/registers of a secret have been erased.

## Example

```c
#define MULIB_IMPL_PORT
#include <mulib-port.h>
#include <assert.h>
int main(void) {
    size_t stride;
    assert(mulib_port_align_size(13, 8, &stride) && stride == 16);
    return 0;
}
```

Compile: `cc -std=c11 -Iinclude examples/modules/port.c -o port`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-port.h`](../include/mulib-port.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-port.h
 * @brief Platform identification and checked size arithmetic for C11 modules.
 * Define MULIB_IMPL_PORT once. No operating-system headers or feature-test
 * macros are exposed. Platform macros are always defined to either 0 or 1.
 */
#ifndef MULIB_PORT_H
#define MULIB_PORT_H
#include <stddef.h>
#if defined(_WIN32)
#define MULIB_PORT_WINDOWS 1
#define MULIB_PORT_POSIX 0
#else
#define MULIB_PORT_WINDOWS 0
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_PORT_POSIX 1
#else
#define MULIB_PORT_POSIX 0
#endif
#endif
#if defined(__linux__)
#define MULIB_PORT_LINUX 1
#else
#define MULIB_PORT_LINUX 0
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return a static descriptive OS name, including "unknown". */
  const char *mulib_port_os (void);
  /** @brief Return a static descriptive architecture name. */
  const char *mulib_port_cpu (void);
  /** @brief Return nonzero on a little-endian host. */
  int mulib_port_little_endian (void);
  /** @brief Checked addition; failure leaves *out unchanged. */
  int mulib_port_add_size (size_t a, size_t b, size_t *out);
  /** @brief Checked multiplication; failure leaves *out unchanged. */
  int mulib_port_mul_size (size_t a, size_t b, size_t *out);
  /** @brief Round up to any nonzero alignment, checking for overflow. */
  int mulib_port_align_size (size_t size, size_t alignment, size_t *out);
  /** @brief Volatile byte erasure; NULL is accepted only for zero length. */
  void mulib_port_secure_zero (void *memory, size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PORT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-port.h`](../include/mulib-port.h) |
| Implementation switch | `MULIB_IMPL_PORT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/port.c`](../examples/modules/port.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/port.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
