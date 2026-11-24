/** @file mulib-packetvm.h
 * @brief Bounded, forward-only packet-filter bytecode interpreter.
 * Define MULIB_IMPL_PACKETVM once. This is a MuLIB VM, not kernel BPF.
 */
#ifndef MULIB_PACKETVM_H
#define MULIB_PACKETVM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_PACKETVM_LD8 = 1,
    MULIB_PACKETVM_LD16,
    MULIB_PACKETVM_LD32,
    MULIB_PACKETVM_AND,
    MULIB_PACKETVM_JEQ,
    MULIB_PACKETVM_RET
  };
  typedef struct mulib_packetvm_instruction
  {
    unsigned opcode;
    uint32_t value;
    size_t yes, no; /* JEQ forward skips, otherwise must be zero */
  } mulib_packetvm_instruction_t;
  /** @brief Validate <=65536 instructions, final RET, forward in-range jumps.
   * RET values are 0 drop/1 accept. 1 valid, 0 invalid; bad is SIZE_MAX if
   * none. */
  int mulib_packetvm_validate (const mulib_packetvm_instruction_t *, size_t,
                               size_t *bad);
  /** @brief Evaluate from zero accumulator; big-endian absolute loads. Returns
   * 0 success with verdict, -1 invalid, -2 budget exhausted, -3 packet too
   * short. Output unchanged on error. No memory writes or external calls. */
  int mulib_packetvm_run (const mulib_packetvm_instruction_t *, size_t,
                          const void *, size_t, size_t budget, int *verdict);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PACKETVM) && !defined(MULIB_PACKETVM_IMPLEMENTED)
#define MULIB_PACKETVM_IMPLEMENTED
int
mulib_packetvm_validate (const mulib_packetvm_instruction_t *p, size_t n,
                         size_t *bad)
{
  if (bad)
    *bad = SIZE_MAX;
  if (!p || !n || n > 65536 || p[n - 1].opcode != MULIB_PACKETVM_RET)
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      int ok = p[i].opcode >= 1 && p[i].opcode <= MULIB_PACKETVM_RET;
      if (p[i].opcode == MULIB_PACKETVM_JEQ)
        ok = ok && p[i].yes < n - i - 1 && p[i].no < n - i - 1;
      else
        ok = ok && !p[i].yes && !p[i].no;
      if (p[i].opcode == MULIB_PACKETVM_RET)
        ok = ok && p[i].value <= 1;
      if (!ok)
        {
          if (bad)
            *bad = i;
          return 0;
        }
    }
  return 1;
}
int
mulib_packetvm_run (const mulib_packetvm_instruction_t *p, size_t n,
                    const void *data, size_t len, size_t budget, int *verdict)
{
  const unsigned char *bytes = data;
  uint32_t a = 0;
  size_t pc = 0;
  if ((!data && len) || !verdict || !mulib_packetvm_validate (p, n, NULL))
    return -1;
  while (pc < n)
    {
      const mulib_packetvm_instruction_t *v = p + pc++;
      if (!budget)
        return -2;
      --budget;
      if (v->opcode <= MULIB_PACKETVM_LD32)
        {
          size_t width = v->opcode == 1 ? 1 : v->opcode == 2 ? 2 : 4;
          if (v->value > len || width > len - v->value)
            return -3;
          a = 0;
          for (size_t i = 0; i < width; ++i)
            a = (a << 8) | bytes[v->value + i];
        }
      else if (v->opcode == MULIB_PACKETVM_AND)
        a &= v->value;
      else if (v->opcode == MULIB_PACKETVM_JEQ)
        pc += a == v->value ? v->yes : v->no;
      else
        {
          *verdict = (int)v->value;
          return 0;
        }
    }
  return -1;
}
#endif
