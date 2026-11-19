# Scalar shader IR and portable specialization

`mulib-glx-shaderir.h`, `mulib-glx-shadervm.h` and `mulib-glx-shaderjit.h`
implement a bounded scalar arithmetic DAG, interpreter, and constant folder.
Despite its legacy filename, `shaderjit` does **not** emit machine code, allocate
executable memory, or implement GPU draws, drivers, resources, or fences.

Initialize owning IR objects empty, do not copy live owning structs, and free
them with `mulib_glx_shaderir_free`. `mulib_glx_shaderir_add` returns a register
index or `-1`. Operands must refer backward, input indices are below 16, and at
most 65,535 instructions are accepted. All four output indices must refer to
existing registers before execution or specialization.

The interpreter requires 16 input floats, four output floats, and at least
`shader.n` scratch floats. Scratch must not overlap output or IR storage. Inputs
are snapshotted and may overlap scratch or output. Invalid referenced inputs or
non-finite arithmetic results fail; output is unchanged and scratch may have
been written. Division by zero retains the original defined result of zero.

Specialization transactionally replaces an initialized output IR, including
an already owning destination or the source itself. A failure leaves it intact.
Only finite constant expressions are folded. This is an optimization of the
same interpreter semantics, not a capability claim for optional GPU loaders.

`tests/test-gfx-shader.c` covers malformed DAG metadata, output/register bounds,
finite-value handling, input/scratch aliasing, portable folding equivalence, and
repeated/in-place ownership replacement.

`mulib-glx-gpu.h` is only a renderer vtable with CPU fallback. It validates
lists and surfaces, and propagates the canonical CPU renderer's failure result
instead of reporting constant success. The regression checks actual fallback
pixels and invalid-command rejection; it is not evidence of hardware support.
