# Secret memory

Initialize the owning buffer, do not copy it, and release it deterministically. POSIX data mappings have guard pages; strict locking fails closed if lock cannot be obtained. Dump/fork advice is best-effort and reported in hardening flags. An owning buffer cannot protect separate stack copies, privileged reads or leaked application logs. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-memory.h`](../kernels/mulib-secret-memory.h) |
| Implementation switch | `MULIB_IMPL_SECRET_MEMORY` |
| Repository-local includes | [mulib-secret-core](mulib-secret-core.md) |
| Complete example | [`examples/mulib-secret-kernels.c`](../examples/mulib-secret-kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/mulib-secret-kernels.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
