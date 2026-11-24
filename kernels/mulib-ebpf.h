/** @file mulib-ebpf.h
 * @brief Bounded register-only eBPF instruction subset interpreter, not kernel
 * BPF. Define MULIB_IMPL_EBPF once. No syscalls, helpers, packet memory, JIT
 * or kernel loading. */
#ifndef MULIB_EBPF_H
#define MULIB_EBPF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ebpf_instruction
  {
    unsigned char opcode, dst, src;
    int16_t offset;
    int32_t immediate;
  } mulib_ebpf_instruction_t;
  /** @brief Validate MOV64 K/X (b7/bf), ADD64 K/X (07/0f), JEQ K (15),
   * forward JA (05), EXIT (95). r0..r9 only, all initially zero. Forward-only
   * jumps, <=65536 instructions, final EXIT required; unused fields must be
   * zero. 1 valid, 0 invalid. This is not the Linux verifier or a sandbox
   * guarantee. */
  int mulib_ebpf_validate (const mulib_ebpf_instruction_t *code, size_t count,
                           size_t *bad_instruction);
  /** @brief Execute validated program, at most max_steps. 0 success, -1
   * invalid, -2 budget exhausted. Returns r0, unchanged on error; ALU uses
   * modulo-2^64. Signed immediates are sign-extended, as in eBPF ALU64. */
  int mulib_ebpf_run (const mulib_ebpf_instruction_t *code, size_t count,
                      size_t max_steps, uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EBPF) && !defined(MULIB_EBPF_IMPLEMENTED)
#define MULIB_EBPF_IMPLEMENTED
int
mulib_ebpf_validate (const mulib_ebpf_instruction_t *p, size_t n, size_t *bad)
{
  size_t i;
  int valid;
  if (bad)
    *bad = SIZE_MAX;
  if (!p || !n || n > 65536 || p[n - 1].opcode != 0x95)
    return 0;
  for (i = 0; i < n; ++i)
    {
      const mulib_ebpf_instruction_t *v = p + i;
      valid = v->dst < 10 && v->src < 10;
      switch (v->opcode)
        {
        case 0xb7:
        case 0x07:
          valid = valid && !v->src && !v->offset;
          break;
        case 0xbf:
        case 0x0f:
          valid = valid && !v->immediate && !v->offset;
          break;
        case 0x15:
          valid = valid && !v->src && v->offset >= 0
                  && (size_t)v->offset < n - i - 1;
          break;
        case 0x05:
          valid = valid && !v->dst && !v->src && !v->immediate
                  && v->offset >= 0 && (size_t)v->offset < n - i - 1;
          break;
        case 0x95:
          valid = valid && !v->dst && !v->src && !v->offset && !v->immediate;
          break;
        default:
          valid = 0;
          break;
        }
      if (!valid)
        {
          if (bad)
            *bad = i;
          return 0;
        }
    }
  return 1;
}
int
mulib_ebpf_run (const mulib_ebpf_instruction_t *p, size_t n, size_t budget,
                uint64_t *out)
{
  uint64_t r[10] = { 0 };
  size_t pc = 0, steps = 0;
  if (!out || !mulib_ebpf_validate (p, n, NULL))
    return -1;
  while (pc < n)
    {
      const mulib_ebpf_instruction_t *v = p + pc;
      if (steps++ == budget)
        return -2;
      ++pc;
      switch (v->opcode)
        {
        case 0xb7:
          r[v->dst] = (uint64_t)(int64_t)v->immediate;
          break;
        case 0xbf:
          r[v->dst] = r[v->src];
          break;
        case 0x07:
          r[v->dst] += (uint64_t)(int64_t)v->immediate;
          break;
        case 0x0f:
          r[v->dst] += r[v->src];
          break;
        case 0x15:
          if (r[v->dst] == (uint64_t)(int64_t)v->immediate)
            pc += (size_t)v->offset;
          break;
        case 0x05:
          pc += (size_t)v->offset;
          break;
        case 0x95:
          *out = r[0];
          return 0;
        }
    }
  return -1;
}
#endif
