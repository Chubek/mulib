/** @file mulib-lowopt.h
 * @brief Semantics-preserving local native MIR simplification and dead writes.
 * Define MULIB_IMPL_LOWOPT and MULIB_IMPL_MIR.
 */
#ifndef MULIB_LOWOPT_H
#define MULIB_LOWOPT_H
#include "mulib-mir.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_lowopt_stats { size_t folded, simplified, removed; }
  mulib_lowopt_stats_t;
/** Evaluate a wrapping native binary operation without signed C overflow.
 * Returns zero for unsupported opcodes; failure leaves result unchanged. */
int mulib_lowopt_evaluate (mulib_mir_native_opcode_t opcode, uint64_t a,
    uint64_t b, uint64_t *result);
/** Clone, fold block-local constants, simplify identities/constant branches,
 * remove self copies and writes to globally unread registers. Memory accesses
 * and calls are never removed or reordered. Removed instructions become NOPs,
 * preserving block/record IDs. Failure preserves output and stats. */
mulib_mir_status_t mulib_lowopt_run (const mulib_mir_native_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out, mulib_lowopt_stats_t *stats,
    mulib_mir_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LOWOPT) && !defined(MULIB_LOWOPT_IMPLEMENTED)
#define MULIB_LOWOPT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int
mulib_lowopt_evaluate (mulib_mir_native_opcode_t op, uint64_t a, uint64_t b, uint64_t *out)
{
  uint64_t v;
  unsigned shift = (unsigned)(b & 63);
  if (!out) return 0;
  switch (op)
    {
    case MULIB_MIR_NATIVE_ADD: v = a + b; break;
    case MULIB_MIR_NATIVE_SUB: v = a - b; break;
    case MULIB_MIR_NATIVE_MUL: v = a * b; break;
    case MULIB_MIR_NATIVE_AND: v = a & b; break;
    case MULIB_MIR_NATIVE_OR: v = a | b; break;
    case MULIB_MIR_NATIVE_XOR: v = a ^ b; break;
    case MULIB_MIR_NATIVE_SHL: v = a << shift; break;
    case MULIB_MIR_NATIVE_SHR: v = a >> shift; break;
    case MULIB_MIR_NATIVE_SAR:
      v = a >> shift;
      if (shift && (a >> 63)) v |= UINT64_MAX << (64 - shift);
      break;
    case MULIB_MIR_NATIVE_EQ: v = a == b; break;
    case MULIB_MIR_NATIVE_SLT: v = (a ^ (UINT64_C(1) << 63)) < (b ^ (UINT64_C(1) << 63)); break;
    case MULIB_MIR_NATIVE_ULT: v = a < b; break;
    default: return 0;
    }
  *out = v; return 1;
}
static void
mulib_lowopt_constant_ (mulib_mir_native_instruction_t *i, uint64_t v)
{
  size_t dst = i->destination;
  *i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_CONST);
  i->destination = dst; i->immediate = v;
}
mulib_mir_status_t
mulib_lowopt_run (const mulib_mir_native_program_t *p, size_t limit,
                 mulib_mir_native_buffer_t *out, mulib_lowopt_stats_t *stats,
                 mulib_mir_diagnostic_t *d)
{
  mulib_mir_native_buffer_t r;
  mulib_lowopt_stats_t st = { 0, 0, 0 };
  unsigned char *known = NULL, *used = NULL;
  uint64_t *values = NULL;
  size_t b, k, j;
  int changed;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!out || out->code || out->blocks || out->arguments || out->symbols
      || out->count || out->block_count || out->register_count || out->parameter_count
      || out->argument_count || out->symbol_count)
    { if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; } return MULIB_MIR_INVALID; }
  s = mulib_mir_native_clone (p, limit, &r, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->register_count)
    {
      known = (unsigned char *)calloc (p->register_count, 1);
      used = (unsigned char *)calloc (p->register_count, 1);
      values = (uint64_t *)calloc (p->register_count, sizeof (*values));
      if (!known || !used || !values) { s = MULIB_MIR_NO_MEMORY; goto done; }
    }
  for (b = 0; b < p->block_count; ++b)
    {
      if (p->register_count) memset (known, 0, p->register_count);
      for (k = p->blocks[b].first; k < p->blocks[b].first + p->blocks[b].count; ++k)
        {
          mulib_mir_native_instruction_t *i = &r.code[k];
          uint64_t v;
          if (i->opcode == MULIB_MIR_NATIVE_COPY && i->a == i->destination)
            { *i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_NOP); ++st.removed; }
          if (i->opcode == MULIB_MIR_NATIVE_COPY && known[i->a])
            { mulib_lowopt_constant_ (i, values[i->a]); ++st.folded; }
          if (i->opcode >= MULIB_MIR_NATIVE_ADD && i->opcode <= MULIB_MIR_NATIVE_ULT)
            {
              if (known[i->a] && known[i->b] && mulib_lowopt_evaluate (i->opcode, values[i->a], values[i->b], &v))
                { mulib_lowopt_constant_ (i, v); ++st.folded; }
              else if (known[i->b]
                       && ((values[i->b] == 0 && (i->opcode == MULIB_MIR_NATIVE_ADD || i->opcode == MULIB_MIR_NATIVE_SUB
                            || i->opcode == MULIB_MIR_NATIVE_OR || i->opcode == MULIB_MIR_NATIVE_XOR))
                           || (values[i->b] == 1 && i->opcode == MULIB_MIR_NATIVE_MUL)
                           || (values[i->b] == UINT64_MAX && i->opcode == MULIB_MIR_NATIVE_AND)
                           || (!(values[i->b] & 63) && i->opcode >= MULIB_MIR_NATIVE_SHL && i->opcode <= MULIB_MIR_NATIVE_SAR)))
                { i->opcode = MULIB_MIR_NATIVE_COPY; i->b = MULIB_MIR_NONE; ++st.simplified; }
              else if (i->a == i->b && (i->opcode == MULIB_MIR_NATIVE_SUB || i->opcode == MULIB_MIR_NATIVE_XOR
                       || i->opcode == MULIB_MIR_NATIVE_EQ || i->opcode == MULIB_MIR_NATIVE_SLT || i->opcode == MULIB_MIR_NATIVE_ULT))
                { mulib_lowopt_constant_ (i, i->opcode == MULIB_MIR_NATIVE_EQ); ++st.simplified; }
            }
          if (i->opcode == MULIB_MIR_NATIVE_BRANCH && known[i->a])
            {
              size_t target = values[i->a] ? i->target : i->b;
              *i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP); i->target = target; ++st.simplified;
            }
          if (mulib_mir_native_defines (i->opcode))
            {
              known[i->destination] = i->opcode == MULIB_MIR_NATIVE_CONST;
              if (known[i->destination]) values[i->destination] = i->immediate;
            }
        }
    }
  do {
    changed = 0;
    if (p->register_count) memset (used, 0, p->register_count);
    for (k = 0; k < r.count; ++k)
      {
        const mulib_mir_native_instruction_t *i = &r.code[k];
        int arity = mulib_mir_native_arity (i->opcode);
        if (arity > 0) used[i->a] = 1;
        if (arity == 2) used[i->b] = 1;
        if (i->opcode == MULIB_MIR_NATIVE_CALL)
          for (j = 0; j < i->b; ++j) used[r.arguments[i->a + j]] = 1;
      }
    for (k = 0; k < r.count; ++k)
      {
        mulib_mir_native_instruction_t *i = &r.code[k];
        if (mulib_mir_native_defines (i->opcode) && !mulib_mir_native_effect (i->opcode) && !used[i->destination])
          { *i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_NOP); ++st.removed; changed = 1; }
      }
  } while (changed);
  *out = r; memset (&r, 0, sizeof (r)); if (stats) *stats = st;
done:
  free (known); free (used); free (values); mulib_mir_native_free (&r);
  if (d) { d->status = s; d->record = 0; } return s;
}
#ifdef __cplusplus
}
#endif
#endif
