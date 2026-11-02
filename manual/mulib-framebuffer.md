# mulib-framebuffer

Define `MULIB_IMPL_FRAMEBUFFER` once; no dependencies or allocation.
[Example](../examples/modules/framebuffer.c).

Surface fields are a borrowed volatile 32-bit pixel pointer, total capacity in
words, visible width/height and row pitch in words. `valid` checks nonzero
dimensions, pitch, alignment and overflow-safe last visible pixel bounds.
The module only supports directly mapped 32-bit pixels; obtain and normalize
GOP/VBE format and mapping details in the application's firmware adapter.

`pattern` fills a two-color checkerboard with a nonzero tile size in pixels;
`verify` reads it back and counts mismatches. Both skip row padding. Colors are
raw device-format words. Pattern returns 0/-1; verify returns `SIZE_MAX` on
invalid input, otherwise mismatch count. Invalid calls never touch pixel storage.

**Destructive and CPU-driven**, not a shader/compute GPU stress test. A real
framebuffer must be mapped writable, and readable for verify. Caller handles
cache flushes, write-combining fences and display synchronization. Some apertures
do not support reliable readback. Never pass ordinary physical addresses without
mapping or assume this tests dedicated VRAM instead of a system-memory shadow.

`mulib_framebuffer_rgb` packs RGB8 into three caller-supplied firmware channel
masks, including RGB565 or RGB/BGR888. Masks must be contiguous, nonzero and
disjoint; conversion scales/rounds each channel using 64-bit arithmetic. Unused
bits are zero. Returns 0 and writes pixel, or -1 without modifying output.
