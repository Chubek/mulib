# mulib-mop

[Manuals](README.md) · [Library index](index.md)

An allocation-free meta object protocol for borrowed class descriptors and
inherited named methods. Define `MULIB_IMPL_MOP` in one translation unit.

Classes contain a name, optional parent and method array. An object borrows its
class and caller-owned payload. All descriptors, names, methods and payload
storage must outlive dispatch. Immutable descriptors may be shared; callers
synchronize access to mutable payloads. The protocol never destroys user data.

`mulib_mop_class_valid` checks the complete chain: at most 64 classes, no cycles,
nonempty names, at most 1024 methods per class, non-NULL callbacks, no duplicate
method names within a class. Derived methods override inherited methods.
`mulib_mop_init` validates before modifying output. `mulib_mop_is_a` tests class
identity/ancestry. `mulib_mop_lookup` borrows the nearest method or returns NULL.
`mulib_mop_call` returns 1 when dispatched and optionally copies the callback's
application-defined status; it returns 0 for missing methods or invalid objects.
Callback arguments/output are borrowed and application-typed. A callback's
failure does not automatically roll back changes it made to the payload.

NULL inputs fail safely. No dynamic registration, automatic field reflection,
allocation, serialization or global mutable registry. See
`examples/modules/mop.c` and `tests/test_new_modules.c`.

`mulib_mop_version()` returns API revision 1.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mop.h`](../include/mulib-mop.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mop.h
 * @brief Borrowed class descriptors, inherited methods and checked dispatch.
 * Define MULIB_IMPL_MOP once. No allocations or global registry.
 */
#ifndef MULIB_MOP_H
#define MULIB_MOP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_mop_version (void);
  typedef struct mulib_mop_object mulib_mop_object_t;
  /** @brief Callback borrows self, argument and output; status is
   * application-defined. */
  typedef int (*mulib_mop_method_fn) (mulib_mop_object_t *self,
                                      const void *argument, void *out);
  typedef struct mulib_mop_method
  {
    const char *name;
    mulib_mop_method_fn call;
  } mulib_mop_method_t;
  /** @brief Immutable descriptors and method/name storage must outlive all
   * objects. Parent chains are acyclic and at most 64 classes deep. Derived
   * methods override.
   */
  typedef struct mulib_mop_class
  {
    const char *name;
    const struct mulib_mop_class *parent;
    const mulib_mop_method_t *methods;
    size_t method_count;
  } mulib_mop_class_t;
  /** @brief Borrowed class and user payload; caller manages lifetime and
   * synchronization. */
  struct mulib_mop_object
  {
    const mulib_mop_class_t *type;
    void *data;
  };
  /** @brief Check the full descriptor chain, including cycles and duplicate
   * methods. */
  int mulib_mop_class_valid (const mulib_mop_class_t *type);
  /** @brief Initialize borrowed object; unchanged on invalid descriptor or
   * NULL output. */
  int mulib_mop_init (mulib_mop_object_t *object,
                      const mulib_mop_class_t *type, void *data);
  /** @brief Return 1 for identity or an ancestor, else 0 (including invalid
   * chains). */
  int mulib_mop_is_a (const mulib_mop_object_t *object,
                      const mulib_mop_class_t *type);
  /** @brief Borrow the nearest named method; NULL for absent/invalid
   * descriptors.
   */
  const mulib_mop_method_t *mulib_mop_lookup (const mulib_mop_class_t *type,
                                              const char *name);
  /** @brief Return 1 if dispatched, 0 if invalid/absent. Optional status
   * receives callback return. */
  int mulib_mop_call (mulib_mop_object_t *object, const char *name,
                      const void *argument, void *out, int *status);
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
| Header | [`include/mulib-mop.h`](../include/mulib-mop.h) |
| Implementation switch | `MULIB_IMPL_MOP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/mop.c`](../examples/modules/mop.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mop.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
