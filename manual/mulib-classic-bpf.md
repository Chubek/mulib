# mulib-classic-bpf

**Attach Linux classic socket BPF instruction arrays to an existing socket**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_CLASSIC_BPF` once before including `mulib-classic-bpf.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_classic_bpf_insn {uint16_t code;uint8_t jt,jf;uint32_t k;} mulib_classic_bpf_insn_t;
int mulib_classic_bpf_attach(int fd,const mulib_classic_bpf_insn_t *insns,size_t n);
```

## Semantics and safety

Attach precompiled cBPF bytecode. Does not compile filter expressions or guarantee that input programs pass kernel verifier. Caller retains fd ownership.

## Demonstration

See `../tests/test_classic-bpf.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-classic-bpf.h`](../include/mulib-classic-bpf.h) |
| Implementation switch | `MULIB_IMPL_CLASSIC_BPF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/classic-bpf.c`](../examples/modules/classic-bpf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/classic-bpf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
