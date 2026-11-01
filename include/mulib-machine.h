/** @file mulib-machine.h
 * @brief Retargetable cost grammars and ELF x86-64 / RV64IM assembly emission.
 * Define MULIB_IMPL_MACHINE, MULIB_IMPL_MIR and MULIB_IMPL_MUBURG.
 */
#ifndef MULIB_MACHINE_H
#define MULIB_MACHINE_H
#include "mulib-mir.h"
#include "mulib-muburg.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_MACHINE_REGISTER = 1024, MULIB_MACHINE_IMMEDIATE, MULIB_MACHINE_WIDE,
  MULIB_MACHINE_REG_GOAL = 0, MULIB_MACHINE_IMM_GOAL = 1
};
/** Selection for one MIR instruction. immediate_mask says which operands are
 * encoded literals rather than virtual-register loads. Other fields are
 * immutable selection results. Cost includes operand loads in this grammar. */
typedef struct mulib_machine_selection {
  unsigned instruction, immediate_mask;
  uint64_t operands[2], cost;
  size_t rule, latency;
} mulib_machine_selection_t;
struct mulib_machine;
/** Target callback receives validated MIR, selected instructions indexed by
 * original record, and a block-local order permutation. It handles register
 * locations, ABI, labels and assembly syntax. Writer returns nonzero on success.
 * A callback error may have produced partial output. */
typedef mulib_mir_status_t (*mulib_machine_emit_fn) (
    const struct mulib_machine *machine, const mulib_mir_native_program_t *program,
    const mulib_machine_selection_t *selected, const size_t *order,
    const char *symbol, mulib_mir_write_fn writer, void *user,
    mulib_mir_diagnostic_t *diagnostic);
/** Borrowed immutable target. Grammar has two goals (register/immediate) and
 * one-terminal-root rules, with native opcodes or the three operand terminals
 * above. Leaf instruction IDs are zero; native rule IDs are target-defined.
 * Latencies are positive and indexed by rule. Ties use grammar order.
 * signed_immediate_bits classifies known operands for semantic legality. */
typedef struct mulib_machine {
  const char *name;
  unsigned word_bits, stack_alignment, signed_immediate_bits;
  const mulib_muburg_rule_t *rules;
  const size_t *latencies;
  size_t rule_count;
  mulib_machine_emit_fn emit;
  const void *context;
} mulib_machine_t;
/** Static built-in target descriptors. Both use 64-bit words, 16-byte stack
 * alignment, complete spill slots and reserved scratch registers. */
const mulib_machine_t *mulib_machine_x86_64_sysv (void);
/** Static RV64IM LP64 descriptor using eight word argument registers. */
const mulib_machine_t *mulib_machine_rv64im (void);
/** Validate target grammar/metadata within a rule-count bound. */
mulib_mir_status_t mulib_machine_validate (const mulib_machine_t *machine, size_t limit);
/** Minimum-cost cover of one instruction and its operand leaves, using
 * MuBURG. known_mask/values describe constants proven at this program point.
 * The optimum is local to this grammar, not a global DAG/assembly optimum.
 * Failure leaves selection unchanged. */
mulib_mir_status_t mulib_machine_select (const mulib_machine_t *machine,
    const mulib_mir_native_instruction_t *instruction, unsigned known_mask,
    const uint64_t values[2], size_t limit, mulib_machine_selection_t *selection);
/** Validate program, target and block-local permutation before emitting.
 * selected must be an unmodified selection for this exact program. */
mulib_mir_status_t mulib_machine_emit (const mulib_machine_t *machine,
    const mulib_mir_native_program_t *program, const mulib_machine_selection_t *selected,
    const size_t *order, const char *symbol, size_t limit, mulib_mir_write_fn writer,
    void *user, mulib_mir_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHINE) && !defined(MULIB_MACHINE_IMPLEMENTED)
#define MULIB_MACHINE_IMPLEMENTED
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_mir_status_t
mulib_machine_validate (const mulib_machine_t *m, size_t limit)
{
  size_t k, j;
  if (!m || !m->name || !m->rules || !m->latencies || !m->rule_count || !m->emit
      || m->word_bits != 64 || !m->stack_alignment
      || (m->stack_alignment & (m->stack_alignment - 1))
      || !m->signed_immediate_bits || m->signed_immediate_bits > 64) return MULIB_MIR_INVALID;
  if (m->rule_count > limit || m->rule_count > SIZE_MAX / sizeof (*m->rules)
      || m->rule_count > SIZE_MAX / sizeof (*m->latencies)) return MULIB_MIR_RESOURCE_LIMIT;
  for (k = 0; k < m->rule_count; ++k)
    {
      const mulib_muburg_rule_t *r = &m->rules[k];
      int arity = mulib_mir_native_arity (r->opcode);
      if (r->lhs > 1 || r->arity > 2 || !m->latencies[k]) return MULIB_MIR_INVALID;
      if (r->opcode >= MULIB_MACHINE_REGISTER && r->opcode <= MULIB_MACHINE_WIDE)
        { if (r->arity || r->instruction) return MULIB_MIR_INVALID; }
      else if (arity < 0 || (unsigned)arity != r->arity || r->lhs != MULIB_MACHINE_REG_GOAL)
        return MULIB_MIR_INVALID;
      for (j = 0; j < r->arity; ++j) if (r->child_goal[j] > 1) return MULIB_MIR_INVALID;
    }
  return MULIB_MIR_OK;
}
mulib_mir_status_t
mulib_machine_select (const mulib_machine_t *m, const mulib_mir_native_instruction_t *i,
                      unsigned mask, const uint64_t values[2], size_t limit,
                      mulib_machine_selection_t *out)
{
  mulib_muburg_node_t nodes[3];
  mulib_muburg_plan_t plan;
  mulib_muburg_limits_t limits;
  mulib_muburg_status_t ms;
  mulib_machine_selection_t r;
  mulib_mir_status_t s = mulib_machine_validate (m, limit);
  int arity, j;
  size_t choice;
  if (s != MULIB_MIR_OK) return s;
  if (!i || !out || (mask && !values) || mask > 3) return MULIB_MIR_INVALID;
  arity = mulib_mir_native_arity (i->opcode);
  if (arity < 0) return MULIB_MIR_BAD_OPCODE;
  if (mask >> arity) return MULIB_MIR_INVALID;
  memset (&plan, 0, sizeof (plan)); memset (&r, 0, sizeof (r)); memset (nodes, 0, sizeof (nodes));
  for (j = 0; j < arity; ++j)
    {
      nodes[j].opcode = MULIB_MACHINE_REGISTER;
      if (mask & (1u << j))
        {
          uint64_t bound = UINT64_C(1) << (m->signed_immediate_bits - 1);
          nodes[j].opcode = values[j] < bound || values[j] >= UINT64_MAX - bound + 1
                              ? MULIB_MACHINE_IMMEDIATE : MULIB_MACHINE_WIDE;
        }
    }
  nodes[arity].opcode = i->opcode; nodes[arity].arity = (unsigned)arity;
  nodes[arity].child[0] = 0; nodes[arity].child[1] = 1;
  limits.nodes = 3; limits.rules = limit; limits.goals = 2; limits.cells = 6;
  ms = mulib_muburg_select (nodes, (size_t)arity + 1, m->rules, m->rule_count,
      2, (size_t)arity, 0, limits, &plan, NULL);
  if (ms != MULIB_MUBURG_OK)
    {
      if (ms == MULIB_MUBURG_NO_MEMORY) return MULIB_MIR_NO_MEMORY;
      if (ms == MULIB_MUBURG_RESOURCE_LIMIT || ms == MULIB_MUBURG_OVERFLOW) return MULIB_MIR_RESOURCE_LIMIT;
      return MULIB_MIR_UNSUPPORTED;
    }
  choice = plan.choices[(size_t)arity * 2];
  r.rule = choice; r.instruction = m->rules[choice].instruction;
  r.latency = m->latencies[choice]; r.cost = plan.cost;
  for (j = 0; j < arity; ++j)
    if (m->rules[choice].child_goal[j] == MULIB_MACHINE_IMM_GOAL)
      {
        if (!(mask & (1u << j))) { mulib_muburg_free (&plan); return MULIB_MIR_INVALID; }
        r.immediate_mask |= 1u << j; r.operands[j] = values[j];
      }
  mulib_muburg_free (&plan); *out = r; return MULIB_MIR_OK;
}
mulib_mir_status_t
mulib_machine_emit (const mulib_machine_t *m, const mulib_mir_native_program_t *p,
                    const mulib_machine_selection_t *selected, const size_t *order,
                    const char *symbol, size_t limit, mulib_mir_write_fn writer,
                    void *user, mulib_mir_diagnostic_t *d)
{
  unsigned char *seen = NULL;
  size_t b, k = 0;
  mulib_mir_status_t s = mulib_machine_validate (m, limit);
  if (s != MULIB_MIR_OK) goto done;
  s = mulib_mir_native_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_INVALID;
  if (!selected || !order || !writer || !mulib_mir_native_name (symbol)) goto done;
  seen = (unsigned char *)calloc (p->count, 1);
  s = MULIB_MIR_NO_MEMORY;
  if (!seen) goto done;
  s = MULIB_MIR_BAD_REFERENCE;
  for (b = 0; b < p->block_count; ++b)
    for (k = p->blocks[b].first; k < p->blocks[b].first + p->blocks[b].count; ++k)
      {
        size_t n = order[k], end = p->blocks[b].first + p->blocks[b].count;
        if (n < p->blocks[b].first || n >= end || seen[n] || ((k == end - 1) != (n == end - 1))) goto done;
        seen[n] = 1;
        if (selected[n].rule >= m->rule_count || selected[n].instruction != m->rules[selected[n].rule].instruction
            || m->rules[selected[n].rule].opcode != (int)p->code[n].opcode) goto done;
      }
  free (seen);
  return m->emit (m, p, selected, order, symbol, writer, user, d);
done:
  free (seen); if (d) { d->status = s; d->record = k; } return s;
}
typedef struct mulib_machine_text_ { mulib_mir_write_fn writer; void *user; int ok; }
  mulib_machine_text_;
static void
mulib_machine_textf_ (mulib_machine_text_ *t, const char *format, ...)
{
  char text[512];
  int n;
  va_list ap;
  if (!t->ok) return;
  va_start (ap, format); n = vsnprintf (text, sizeof (text), format, ap); va_end (ap);
  if (n < 0 || (size_t)n >= sizeof (text) || !t->writer (t->user, text, (size_t)n)) t->ok = 0;
}
static int64_t mulib_machine_signed_ (uint64_t v)
{ return v <= INT64_MAX ? (int64_t)v : -1 - (int64_t)(UINT64_MAX - v); }
static void
mulib_machine_rv_slot_ (mulib_machine_text_ *t, int store, const char *reg,
                        const char *base, int64_t offset)
{
  if (offset >= -2048 && offset <= 2047)
    mulib_machine_textf_ (t, "\t%s %s, %" PRId64 "(%s)\n", store ? "sd" : "ld", reg, offset, base);
  else
    mulib_machine_textf_ (t, "\tli t2, %" PRId64 "\n\tadd t2, %s, t2\n\t%s %s, 0(t2)\n",
                         offset, base, store ? "sd" : "ld", reg);
}
static mulib_mir_status_t
mulib_machine_builtin_emit_ (const mulib_machine_t *m, const mulib_mir_native_program_t *p,
                            const mulib_machine_selection_t *sel, const size_t *order,
                            const char *name, mulib_mir_write_fn writer, void *user,
                            mulib_mir_diagnostic_t *d)
{
  static const char *const xargs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
  static const char *const rargs[] = { "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" };
  static const char *const xops[] = { "addq", "subq", "imulq", "andq", "orq", "xorq", "shlq", "shrq", "sarq" };
  static const char *const rops[] = { "add", "sub", "mul", "and", "or", "xor", "sll", "srl", "sra" };
  static const char *const riops[] = { "addi", NULL, NULL, "andi", "ori", "xori", "slli", "srli", "srai" };
  mulib_machine_text_ text;
  size_t b, k = 0, j, frame, slots, argc = m->context ? 8 : 6;
  int rv = m->context != NULL;
  mulib_mir_status_t s = MULIB_MIR_RESOURCE_LIMIT;
  text.writer = writer; text.user = user; text.ok = 1;
  if (m->stack_alignment != 16 || m->signed_immediate_bits > (rv ? 12u : 32u))
    { s = MULIB_MIR_UNSUPPORTED; goto done; }
  /* Bound all frame and outgoing offsets to signed 32 bits, independently of
   * host size_t. This also bounds x86 displacement/immediate encodings. */
  if (p->register_count > (size_t)(INT32_MAX - 31) / 8
      || p->parameter_count > (size_t)(INT32_MAX - 31) / 8 - p->register_count) goto done;
  slots = p->register_count + p->parameter_count; frame = (slots * 8 + 15) & ~(size_t)15;
  for (k = 0; k < p->count; ++k)
    if (p->code[k].opcode == MULIB_MIR_NATIVE_CALL && p->code[k].b > (size_t)(INT32_MAX - 31) / 8) goto done;
  mulib_machine_textf_ (&text, "\t.text\n\t.globl %s\n\t.type %s, @function\n%s:\n", name, name, name);
  if (rv)
    mulib_machine_textf_ (&text, "\taddi sp, sp, -16\n\tsd ra, 8(sp)\n\tsd s0, 0(sp)\n\tmv s0, sp\n\tli t0, %zu\n\tsub sp, sp, t0\n\tli t0, 0\n", frame);
  else
    mulib_machine_textf_ (&text, "\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n\tsubq $%zu, %%rsp\n\txorl %%eax, %%eax\n", frame);
  for (k = 0; k < p->register_count; ++k)
    if (rv) mulib_machine_rv_slot_ (&text, 1, "t0", "s0", -(int64_t)((k + 1) * 8));
    else mulib_machine_textf_ (&text, "\tmovq %%rax, -%zu(%%rbp)\n", (k + 1) * 8);
  for (k = 0; k < p->parameter_count; ++k)
    {
      int64_t offset = -(int64_t)((p->register_count + k + 1) * 8);
      if (rv)
        {
          if (k >= argc) mulib_machine_rv_slot_ (&text, 0, "t0", "s0", (int64_t)(16 + (k - argc) * 8));
          mulib_machine_rv_slot_ (&text, 1, k < argc ? rargs[k] : "t0", "s0", offset);
        }
      else
        {
          if (k >= argc) mulib_machine_textf_ (&text, "\tmovq %zu(%%rbp), %%rax\n", 16 + (k - argc) * 8);
          mulib_machine_textf_ (&text, "\tmovq %s, %" PRId64 "(%%rbp)\n", k < argc ? xargs[k] : "%rax", offset);
        }
    }
  for (b = 0; b < p->block_count && text.ok; ++b)
    {
      mulib_machine_textf_ (&text, ".Lmulib_%s_%zu:\n", name, b);
      for (j = p->blocks[b].first; j < p->blocks[b].first + p->blocks[b].count && text.ok; ++j)
        {
          const mulib_mir_native_instruction_t *i;
          const mulib_machine_selection_t *q;
          int arity, imm;
          int64_t literal;
          k = order[j]; i = &p->code[k]; q = &sel[k];
          s = MULIB_MIR_INVALID;
          if (q->instruction != (unsigned)i->opcode + 1 || (q->immediate_mask & ~2u)) goto done;
          arity = mulib_mir_native_arity (i->opcode); imm = (q->immediate_mask & 2) != 0;
          literal = mulib_machine_signed_ (q->operands[1]);
          if (arity > 0)
            {
              if (rv) mulib_machine_rv_slot_ (&text, 0, "t0", "s0", -(int64_t)((i->a + 1) * 8));
              else mulib_machine_textf_ (&text, "\tmovq -%zu(%%rbp), %%rax\n", (i->a + 1) * 8);
            }
          if (arity == 2 && !imm)
            {
              if (rv) mulib_machine_rv_slot_ (&text, 0, "t1", "s0", -(int64_t)((i->b + 1) * 8));
              else mulib_machine_textf_ (&text, "\tmovq -%zu(%%rbp), %%rcx\n", (i->b + 1) * 8);
            }
          switch (i->opcode)
            {
            case MULIB_MIR_NATIVE_NOP: case MULIB_MIR_NATIVE_COPY: break;
            case MULIB_MIR_NATIVE_CONST:
              mulib_machine_textf_ (&text, rv ? "\tli t0, 0x%016" PRIx64 "\n" : "\tmovabsq $0x%016" PRIx64 ", %%rax\n", i->immediate); break;
            case MULIB_MIR_NATIVE_PARAM:
              if (rv) mulib_machine_rv_slot_ (&text, 0, "t0", "s0", -(int64_t)((p->register_count + i->a + 1) * 8));
              else mulib_machine_textf_ (&text, "\tmovq -%zu(%%rbp), %%rax\n", (p->register_count + i->a + 1) * 8);
              break;
            case MULIB_MIR_NATIVE_ADD: case MULIB_MIR_NATIVE_SUB: case MULIB_MIR_NATIVE_MUL:
            case MULIB_MIR_NATIVE_AND: case MULIB_MIR_NATIVE_OR: case MULIB_MIR_NATIVE_XOR:
            case MULIB_MIR_NATIVE_SHL: case MULIB_MIR_NATIVE_SHR: case MULIB_MIR_NATIVE_SAR:
              {
                size_t op = (size_t)(i->opcode - MULIB_MIR_NATIVE_ADD);
                int shift = i->opcode >= MULIB_MIR_NATIVE_SHL;
                if (shift) literal = (int64_t)(q->operands[1] & 63);
                if (rv)
                  {
                    if (imm && !riops[op]) goto done;
                    if (imm) mulib_machine_textf_ (&text, "\t%s t0, t0, %" PRId64 "\n", riops[op], literal);
                    else mulib_machine_textf_ (&text, "\t%s t0, t0, t1\n", rops[op]);
                  }
                else if (imm) mulib_machine_textf_ (&text, "\t%s $%" PRId64 ", %%rax\n", xops[op], literal);
                else mulib_machine_textf_ (&text, "\t%s %s, %%rax\n", xops[op], shift ? "%cl" : "%rcx");
              }
              break;
            case MULIB_MIR_NATIVE_EQ: case MULIB_MIR_NATIVE_SLT: case MULIB_MIR_NATIVE_ULT:
              if (rv)
                {
                  if (imm) goto done;
                  if (i->opcode == MULIB_MIR_NATIVE_EQ) mulib_machine_textf_ (&text, "\txor t0, t0, t1\n\tseqz t0, t0\n");
                  else mulib_machine_textf_ (&text, "\t%s t0, t0, t1\n", i->opcode == MULIB_MIR_NATIVE_SLT ? "slt" : "sltu");
                }
              else
                {
                  if (imm) mulib_machine_textf_ (&text, "\tcmpq $%" PRId64 ", %%rax\n", literal);
                  else mulib_machine_textf_ (&text, "\tcmpq %%rcx, %%rax\n");
                  mulib_machine_textf_ (&text, "\t%s %%al\n\tmovzbq %%al, %%rax\n",
                      i->opcode == MULIB_MIR_NATIVE_EQ ? "sete" : i->opcode == MULIB_MIR_NATIVE_SLT ? "setl" : "setb");
                }
              break;
            case MULIB_MIR_NATIVE_LOAD:
              mulib_machine_textf_ (&text, rv ? "\tld t0, 0(t0)\n" : "\tmovq (%%rax), %%rax\n"); break;
            case MULIB_MIR_NATIVE_STORE:
              mulib_machine_textf_ (&text, rv ? "\tsd t1, 0(t0)\n" : "\tmovq %%rcx, (%%rax)\n"); break;
            case MULIB_MIR_NATIVE_CALL:
              {
                size_t a, stack = i->b > argc ? ((i->b - argc) * 8 + 15) & ~(size_t)15 : 0;
                if (stack)
                  {
                    if (rv) mulib_machine_textf_ (&text, "\tli t0, %zu\n\tsub sp, sp, t0\n", stack);
                    else mulib_machine_textf_ (&text, "\tsubq $%zu, %%rsp\n", stack);
                  }
                for (a = argc; a < i->b; ++a)
                  {
                    size_t reg = p->arguments[i->a + a];
                    if (rv)
                      { mulib_machine_rv_slot_ (&text, 0, "t0", "s0", -(int64_t)((reg + 1) * 8)); mulib_machine_rv_slot_ (&text, 1, "t0", "sp", (int64_t)((a - argc) * 8)); }
                    else mulib_machine_textf_ (&text, "\tmovq -%zu(%%rbp), %%rax\n\tmovq %%rax, %zu(%%rsp)\n", (reg + 1) * 8, (a - argc) * 8);
                  }
                for (a = 0; a < i->b && a < argc; ++a)
                  {
                    size_t reg = p->arguments[i->a + a];
                    if (rv) mulib_machine_rv_slot_ (&text, 0, rargs[a], "s0", -(int64_t)((reg + 1) * 8));
                    else mulib_machine_textf_ (&text, "\tmovq -%zu(%%rbp), %s\n", (reg + 1) * 8, xargs[a]);
                  }
                mulib_machine_textf_ (&text, "\tcall %s\n", p->symbols[i->target]);
                if (rv)
                  { if (stack) mulib_machine_textf_ (&text, "\tli t0, %zu\n\tadd sp, sp, t0\n", stack); mulib_machine_textf_ (&text, "\tmv t0, a0\n"); }
                else if (stack) mulib_machine_textf_ (&text, "\taddq $%zu, %%rsp\n", stack);
              }
              break;
            case MULIB_MIR_NATIVE_JUMP:
              mulib_machine_textf_ (&text, rv ? "\ttail .Lmulib_%s_%zu\n" : "\tjmp .Lmulib_%s_%zu\n", name, i->target); break;
            case MULIB_MIR_NATIVE_BRANCH:
              if (rv)
                {
                  /* Conditional branches have only a 4 KiB reach. Keep the
                   * conditional edge local and let tail's AUIPC/JALR pair
                   * reach the actual block (also avoiding JAL's 1 MiB limit).
                   * T1 is scratch and RA is not changed by tail. */
                  mulib_machine_textf_ (&text, "\tbeqz t0, .Lmulib_%s_false_%zu\n", name, k);
                  mulib_machine_textf_ (&text, "\ttail .Lmulib_%s_%zu\n", name, i->target);
                  mulib_machine_textf_ (&text, ".Lmulib_%s_false_%zu:\n", name, k);
                  mulib_machine_textf_ (&text, "\ttail .Lmulib_%s_%zu\n", name, i->b);
                }
              else mulib_machine_textf_ (&text, "\ttestq %%rax, %%rax\n\tjne .Lmulib_%s_%zu\n\tjmp .Lmulib_%s_%zu\n", name, i->target, name, i->b);
              break;
            case MULIB_MIR_NATIVE_RETURN:
              mulib_machine_textf_ (&text, rv ? "\tmv a0, t0\n\tmv sp, s0\n\tld s0, 0(sp)\n\tld ra, 8(sp)\n\taddi sp, sp, 16\n\tret\n" : "\tleave\n\tret\n"); break;
            default: s = MULIB_MIR_UNSUPPORTED; goto done;
            }
          if (mulib_mir_native_defines (i->opcode))
            {
              if (rv) mulib_machine_rv_slot_ (&text, 1, "t0", "s0", -(int64_t)((i->destination + 1) * 8));
              else mulib_machine_textf_ (&text, "\tmovq %%rax, -%zu(%%rbp)\n", (i->destination + 1) * 8);
            }
        }
    }
  mulib_machine_textf_ (&text, "\t.size %s, .-%s\n\t.section .note.GNU-stack,\"\"\n", name, name);
  s = text.ok ? MULIB_MIR_OK : MULIB_MIR_IO_ERROR;
done:
  if (d) { d->status = s; d->record = k; } return s;
}
/* Rule IDs are opcode+1; zero is reserved for operand leaves. */
#define MULIB_MACHINE_RULE_(op,a,c) {0,MULIB_MIR_NATIVE_##op,a,{0,0},c,MULIB_MIR_NATIVE_##op+1}
#define MULIB_MACHINE_IMM_(op) {0,MULIB_MIR_NATIVE_##op,2,{0,1},2,MULIB_MIR_NATIVE_##op+1}
#define MULIB_MACHINE_BASE_RULES_ \
  {0,MULIB_MACHINE_REGISTER,0,{0,0},1,0}, \
  {0,MULIB_MACHINE_IMMEDIATE,0,{0,0},1,0}, \
  {1,MULIB_MACHINE_IMMEDIATE,0,{0,0},0,0}, \
  {0,MULIB_MACHINE_WIDE,0,{0,0},1,0}, \
  MULIB_MACHINE_RULE_(NOP,0,0), MULIB_MACHINE_RULE_(CONST,0,2), \
  MULIB_MACHINE_RULE_(PARAM,0,2), MULIB_MACHINE_RULE_(COPY,1,1), \
  MULIB_MACHINE_RULE_(ADD,2,2), MULIB_MACHINE_RULE_(SUB,2,2), \
  MULIB_MACHINE_RULE_(MUL,2,2), MULIB_MACHINE_RULE_(AND,2,2), \
  MULIB_MACHINE_RULE_(OR,2,2), MULIB_MACHINE_RULE_(XOR,2,2), \
  MULIB_MACHINE_RULE_(SHL,2,2), MULIB_MACHINE_RULE_(SHR,2,2), \
  MULIB_MACHINE_RULE_(SAR,2,2), MULIB_MACHINE_RULE_(EQ,2,3), \
  MULIB_MACHINE_RULE_(SLT,2,3), MULIB_MACHINE_RULE_(ULT,2,3), \
  MULIB_MACHINE_RULE_(LOAD,1,2), MULIB_MACHINE_RULE_(STORE,2,1), \
  MULIB_MACHINE_RULE_(CALL,0,4), MULIB_MACHINE_RULE_(JUMP,0,1), \
  MULIB_MACHINE_RULE_(BRANCH,1,3), MULIB_MACHINE_RULE_(RETURN,1,2)
static const mulib_muburg_rule_t mulib_machine_xrules_[] = {
  MULIB_MACHINE_BASE_RULES_, MULIB_MACHINE_IMM_(ADD), MULIB_MACHINE_IMM_(SUB),
  MULIB_MACHINE_IMM_(MUL), MULIB_MACHINE_IMM_(AND), MULIB_MACHINE_IMM_(OR),
  MULIB_MACHINE_IMM_(XOR), MULIB_MACHINE_IMM_(SHL), MULIB_MACHINE_IMM_(SHR),
  MULIB_MACHINE_IMM_(SAR), MULIB_MACHINE_IMM_(EQ), MULIB_MACHINE_IMM_(SLT), MULIB_MACHINE_IMM_(ULT)
};
static const mulib_muburg_rule_t mulib_machine_rrules_[] = {
  MULIB_MACHINE_BASE_RULES_, MULIB_MACHINE_IMM_(ADD), MULIB_MACHINE_IMM_(AND),
  MULIB_MACHINE_IMM_(OR), MULIB_MACHINE_IMM_(XOR), MULIB_MACHINE_IMM_(SHL),
  MULIB_MACHINE_IMM_(SHR), MULIB_MACHINE_IMM_(SAR)
};
#undef MULIB_MACHINE_BASE_RULES_
#undef MULIB_MACHINE_IMM_
#undef MULIB_MACHINE_RULE_
static const size_t mulib_machine_xlatencies_[] = {
  1,1,1,1, 1,1,1,1, 1,1,3,1,1,1,1,1,1,1,1,1, 4,1,8,1,1,1,
  1,1,3,1,1,1,1,1,1,1,1,1
};
static const size_t mulib_machine_rlatencies_[] = {
  1,1,1,1, 1,1,1,1, 1,1,3,1,1,1,1,1,1,1,1,1, 4,1,8,1,1,1,
  1,1,1,1,1,1,1
};
const mulib_machine_t *mulib_machine_x86_64_sysv (void)
{
  static const mulib_machine_t m = { "x86_64-sysv-elf", 64,16,32,
    mulib_machine_xrules_, mulib_machine_xlatencies_,
    sizeof (mulib_machine_xrules_) / sizeof (mulib_machine_xrules_[0]), mulib_machine_builtin_emit_, NULL };
  return &m;
}
const mulib_machine_t *mulib_machine_rv64im (void)
{
  static const int rv = 1;
  static const mulib_machine_t m = { "riscv64im-lp64-elf", 64,16,12,
    mulib_machine_rrules_, mulib_machine_rlatencies_,
    sizeof (mulib_machine_rrules_) / sizeof (mulib_machine_rrules_[0]), mulib_machine_builtin_emit_, &rv };
  return &m;
}
#ifdef __cplusplus
}
#endif
#endif
