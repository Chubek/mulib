/** @file mulib-peephole.h
 * @brief Guarded shallow-tree rewriting for wrapping 64-bit register LIR.
 * Define MULIB_IMPL_PEEPHOLE, MULIB_IMPL_LOWOPT and MULIB_IMPL_MIR.
 */
#ifndef MULIB_PEEPHOLE_H
#define MULIB_PEEPHOLE_H
#include "mulib-lowopt.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Facts must describe operands at this exact program point. left/right are
 * optional pure reaching definitions whose input registers still have the same
 * values. The caller proves this stability; NULL disables nested matches. */
typedef struct mulib_peephole_facts {
  unsigned known;
  uint64_t values[2];
  const mulib_mir_native_instruction_t *left, *right;
} mulib_peephole_facts_t;
typedef enum mulib_peephole_guard {
  MULIB_PEEPHOLE_FOLD, MULIB_PEEPHOLE_A, MULIB_PEEPHOLE_B,
  MULIB_PEEPHOLE_SAME, MULIB_PEEPHOLE_SHIFT_ZERO,
  MULIB_PEEPHOLE_SELF_COPY, MULIB_PEEPHOLE_BRANCH_SAME,
  MULIB_PEEPHOLE_BRANCH_TRUE, MULIB_PEEPHOLE_BRANCH_FALSE,
  MULIB_PEEPHOLE_LEFT_A, MULIB_PEEPHOLE_LEFT_B,
  MULIB_PEEPHOLE_RIGHT_A, MULIB_PEEPHOLE_RIGHT_B
} mulib_peephole_guard_t;
typedef enum mulib_peephole_action {
  MULIB_PEEPHOLE_EVAL, MULIB_PEEPHOLE_COPY_A, MULIB_PEEPHOLE_COPY_B,
  MULIB_PEEPHOLE_ZERO, MULIB_PEEPHOLE_ONE, MULIB_PEEPHOLE_ALL,
  MULIB_PEEPHOLE_DOUBLE_A, MULIB_PEEPHOLE_DOUBLE_B,
  MULIB_PEEPHOLE_NOP, MULIB_PEEPHOLE_YES, MULIB_PEEPHOLE_NO,
  MULIB_PEEPHOLE_LEFT_OTHER, MULIB_PEEPHOLE_RIGHT_OTHER
} mulib_peephole_action_t;
/** A two-level tree pattern: root opcode, a fact/literal/repeated-value guard,
 * optional nested opcode, and replacement. First matching rule wins. */
typedef struct mulib_peephole_rule {
  const char *name;
  mulib_mir_native_opcode_t opcode;
  mulib_peephole_guard_t guard;
  uint64_t literal;
  mulib_mir_native_opcode_t nested;
  mulib_peephole_action_t action;
} mulib_peephole_rule_t;
/** Static built-in catalogue; count is optional. No allocation. */
const mulib_peephole_rule_t *mulib_peephole_rules (size_t *count);
/** Validate rule structure, operand/guard/action compatibility and pure roots.
 * Custom rule equivalence and supplied reaching-definition facts remain the
 * caller's responsibility. Returns 1 for a valid catalogue, 0 otherwise. */
int mulib_peephole_validate (const mulib_peephole_rule_t *, size_t count);
/** Apply a trusted rule catalogue to one instruction. Returns 1 on rewrite,
 * 0 on no match, -1 on invalid arguments. No mutation on no-match/failure.
 * Built-in rules are proven for native MIR's wrapping arithmetic, masked shifts
 * and unsigned Boolean comparisons. Custom rules are the caller's responsibility. */
int mulib_peephole_apply (mulib_mir_native_instruction_t *, const mulib_peephole_facts_t *,
    const mulib_peephole_rule_t *, size_t count, size_t *matched);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PEEPHOLE) && !defined(MULIB_PEEPHOLE_IMPLEMENTED)
#define MULIB_PEEPHOLE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_PEEPHOLE_RULE_(name,op,guard,value,nested,action) \
  {name,MULIB_MIR_NATIVE_##op,MULIB_PEEPHOLE_##guard,value,MULIB_MIR_NATIVE_##nested,MULIB_PEEPHOLE_##action}
static const mulib_peephole_rule_t mulib_peephole_catalogue_[] = {
  MULIB_PEEPHOLE_RULE_("fold-add",ADD,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-sub",SUB,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-mul",MUL,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-and",AND,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-or",OR,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-xor",XOR,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-shl",SHL,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-shr",SHR,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-sar",SAR,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-eq",EQ,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-slt",SLT,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-ult",ULT,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("fold-copy",COPY,FOLD,0,NOP,EVAL),
  MULIB_PEEPHOLE_RULE_("add-zero-right",ADD,B,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("add-zero-left",ADD,A,0,NOP,COPY_B),
  MULIB_PEEPHOLE_RULE_("sub-zero",SUB,B,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("mul-zero-right",MUL,B,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("mul-zero-left",MUL,A,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("mul-one-right",MUL,B,1,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("mul-one-left",MUL,A,1,NOP,COPY_B),
  MULIB_PEEPHOLE_RULE_("and-zero-right",AND,B,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("and-zero-left",AND,A,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("and-all-right",AND,B,UINT64_MAX,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("and-all-left",AND,A,UINT64_MAX,NOP,COPY_B),
  MULIB_PEEPHOLE_RULE_("or-zero-right",OR,B,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-zero-left",OR,A,0,NOP,COPY_B),
  MULIB_PEEPHOLE_RULE_("or-all-right",OR,B,UINT64_MAX,NOP,ALL),
  MULIB_PEEPHOLE_RULE_("or-all-left",OR,A,UINT64_MAX,NOP,ALL),
  MULIB_PEEPHOLE_RULE_("xor-zero-right",XOR,B,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("xor-zero-left",XOR,A,0,NOP,COPY_B),
  MULIB_PEEPHOLE_RULE_("shl-zero-count",SHL,SHIFT_ZERO,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("shr-zero-count",SHR,SHIFT_ZERO,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("sar-zero-count",SAR,SHIFT_ZERO,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("shl-zero-value",SHL,A,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("shr-zero-value",SHR,A,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("sar-zero-value",SAR,A,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("sar-all-value",SAR,A,UINT64_MAX,NOP,ALL),
  MULIB_PEEPHOLE_RULE_("sub-self",SUB,SAME,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("xor-self",XOR,SAME,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("eq-self",EQ,SAME,0,NOP,ONE),
  MULIB_PEEPHOLE_RULE_("slt-self",SLT,SAME,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("ult-self",ULT,SAME,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("and-self",AND,SAME,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-self",OR,SAME,0,NOP,COPY_A),
  MULIB_PEEPHOLE_RULE_("mul-two-right",MUL,B,2,NOP,DOUBLE_A),
  MULIB_PEEPHOLE_RULE_("mul-two-left",MUL,A,2,NOP,DOUBLE_B),
  MULIB_PEEPHOLE_RULE_("shl-one",SHL,B,1,NOP,DOUBLE_A),
  MULIB_PEEPHOLE_RULE_("ult-zero",ULT,B,0,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("ult-max",ULT,A,UINT64_MAX,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("slt-min",SLT,B,(UINT64_C(1)<<63),NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("slt-max",SLT,A,INT64_MAX,NOP,ZERO),
  MULIB_PEEPHOLE_RULE_("branch-true",BRANCH,BRANCH_TRUE,0,NOP,YES),
  MULIB_PEEPHOLE_RULE_("branch-false",BRANCH,BRANCH_FALSE,0,NOP,NO),
  MULIB_PEEPHOLE_RULE_("branch-same",BRANCH,BRANCH_SAME,0,NOP,YES),
  MULIB_PEEPHOLE_RULE_("copy-self",COPY,SELF_COPY,0,NOP,NOP),
  MULIB_PEEPHOLE_RULE_("and-idempotent-a",AND,LEFT_A,0,AND,COPY_A),
  MULIB_PEEPHOLE_RULE_("and-idempotent-b",AND,LEFT_B,0,AND,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-idempotent-a",OR,LEFT_A,0,OR,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-idempotent-b",OR,LEFT_B,0,OR,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-absorb-left-a",OR,LEFT_A,0,AND,COPY_B),
  MULIB_PEEPHOLE_RULE_("or-absorb-left-b",OR,LEFT_B,0,AND,COPY_B),
  MULIB_PEEPHOLE_RULE_("or-absorb-right-a",OR,RIGHT_A,0,AND,COPY_A),
  MULIB_PEEPHOLE_RULE_("or-absorb-right-b",OR,RIGHT_B,0,AND,COPY_A),
  MULIB_PEEPHOLE_RULE_("and-absorb-left-a",AND,LEFT_A,0,OR,COPY_B),
  MULIB_PEEPHOLE_RULE_("and-absorb-left-b",AND,LEFT_B,0,OR,COPY_B),
  MULIB_PEEPHOLE_RULE_("and-absorb-right-a",AND,RIGHT_A,0,OR,COPY_A),
  MULIB_PEEPHOLE_RULE_("and-absorb-right-b",AND,RIGHT_B,0,OR,COPY_A),
  MULIB_PEEPHOLE_RULE_("xor-cancel-left-a",XOR,LEFT_A,0,XOR,LEFT_OTHER),
  MULIB_PEEPHOLE_RULE_("xor-cancel-left-b",XOR,LEFT_B,0,XOR,LEFT_OTHER),
  MULIB_PEEPHOLE_RULE_("xor-cancel-right-a",XOR,RIGHT_A,0,XOR,RIGHT_OTHER),
  MULIB_PEEPHOLE_RULE_("xor-cancel-right-b",XOR,RIGHT_B,0,XOR,RIGHT_OTHER),
  MULIB_PEEPHOLE_RULE_("sub-add-cancel-a",SUB,LEFT_A,0,ADD,LEFT_OTHER),
  MULIB_PEEPHOLE_RULE_("sub-add-cancel-b",SUB,LEFT_B,0,ADD,LEFT_OTHER),
  MULIB_PEEPHOLE_RULE_("add-sub-cancel-left",ADD,LEFT_B,0,SUB,LEFT_OTHER),
  MULIB_PEEPHOLE_RULE_("add-sub-cancel-right",ADD,RIGHT_B,0,SUB,RIGHT_OTHER)
};
#undef MULIB_PEEPHOLE_RULE_
const mulib_peephole_rule_t *mulib_peephole_rules (size_t *count)
{
  if (count) *count = sizeof (mulib_peephole_catalogue_) / sizeof (*mulib_peephole_catalogue_);
  return mulib_peephole_catalogue_;
}
static int mulib_peephole_binary_ (int op)
{ return op >= MULIB_MIR_NATIVE_ADD && op <= MULIB_MIR_NATIVE_ULT; }
int mulib_peephole_validate (const mulib_peephole_rule_t *rules, size_t count)
{
  if ((!rules && count) || count > SIZE_MAX / sizeof (*rules)) return 0;
  for (size_t k = 0; k < count; ++k) {
    const mulib_peephole_rule_t *r = &rules[k];
    int arity = mulib_mir_native_arity (r->opcode), nested = 0;
    if (!r->name || !*r->name || (r->opcode != MULIB_MIR_NATIVE_COPY
        && !mulib_peephole_binary_ (r->opcode) && r->opcode != MULIB_MIR_NATIVE_BRANCH)) return 0;
    switch (r->guard) {
    case MULIB_PEEPHOLE_FOLD: if (r->opcode == MULIB_MIR_NATIVE_BRANCH) return 0; break;
    case MULIB_PEEPHOLE_A: if (arity < 1) return 0; break;
    case MULIB_PEEPHOLE_B: case MULIB_PEEPHOLE_SAME: if (arity != 2) return 0; break;
    case MULIB_PEEPHOLE_SHIFT_ZERO:
      if (r->opcode < MULIB_MIR_NATIVE_SHL || r->opcode > MULIB_MIR_NATIVE_SAR) return 0;
      break;
    case MULIB_PEEPHOLE_SELF_COPY: if (r->opcode != MULIB_MIR_NATIVE_COPY) return 0; break;
    case MULIB_PEEPHOLE_BRANCH_SAME: case MULIB_PEEPHOLE_BRANCH_TRUE: case MULIB_PEEPHOLE_BRANCH_FALSE:
      if (r->opcode != MULIB_MIR_NATIVE_BRANCH) return 0;
      break;
    case MULIB_PEEPHOLE_LEFT_A: case MULIB_PEEPHOLE_LEFT_B:
    case MULIB_PEEPHOLE_RIGHT_A: case MULIB_PEEPHOLE_RIGHT_B:
      if (arity != 2 || !mulib_peephole_binary_ (r->nested)) return 0;
      nested = 1; break;
    default: return 0;
    }
    if ((!nested && r->nested != MULIB_MIR_NATIVE_NOP)
        || (r->guard != MULIB_PEEPHOLE_A && r->guard != MULIB_PEEPHOLE_B && r->literal)) return 0;
    if (r->opcode == MULIB_MIR_NATIVE_BRANCH) {
      if (r->action != MULIB_PEEPHOLE_YES && r->action != MULIB_PEEPHOLE_NO) return 0;
      continue;
    }
    switch (r->action) {
    case MULIB_PEEPHOLE_EVAL: if (r->guard != MULIB_PEEPHOLE_FOLD) return 0; break;
    case MULIB_PEEPHOLE_COPY_A: case MULIB_PEEPHOLE_DOUBLE_A: if (arity < 1) return 0; break;
    case MULIB_PEEPHOLE_COPY_B: case MULIB_PEEPHOLE_DOUBLE_B: if (arity != 2) return 0; break;
    case MULIB_PEEPHOLE_ZERO: case MULIB_PEEPHOLE_ONE: case MULIB_PEEPHOLE_ALL: break;
    case MULIB_PEEPHOLE_NOP: if (r->guard != MULIB_PEEPHOLE_SELF_COPY) return 0; break;
    case MULIB_PEEPHOLE_LEFT_OTHER:
      if (r->guard != MULIB_PEEPHOLE_LEFT_A && r->guard != MULIB_PEEPHOLE_LEFT_B) return 0;
      break;
    case MULIB_PEEPHOLE_RIGHT_OTHER:
      if (r->guard != MULIB_PEEPHOLE_RIGHT_A && r->guard != MULIB_PEEPHOLE_RIGHT_B) return 0;
      break;
    default: return 0;
    }
  }
  return 1;
}
static int mulib_peephole_instruction_ (const mulib_mir_native_instruction_t *i)
{
  int arity = mulib_mir_native_arity (i->opcode);
  if (arity < 0 || (mulib_mir_native_defines (i->opcode) != (i->destination != SIZE_MAX))) return 0;
  if (i->opcode == MULIB_MIR_NATIVE_CALL)
    return i->a != SIZE_MAX && i->b != SIZE_MAX && i->target != SIZE_MAX && !i->immediate;
  if (i->opcode == MULIB_MIR_NATIVE_BRANCH)
    return i->a != SIZE_MAX && i->b != SIZE_MAX && i->target != SIZE_MAX && !i->immediate;
  if (i->opcode == MULIB_MIR_NATIVE_JUMP)
    return i->a == SIZE_MAX && i->b == SIZE_MAX && i->target != SIZE_MAX && !i->immediate;
  if (i->target != SIZE_MAX || (i->opcode != MULIB_MIR_NATIVE_CONST && i->immediate)) return 0;
  if (i->opcode == MULIB_MIR_NATIVE_PARAM) return i->a != SIZE_MAX && i->b == SIZE_MAX;
  return (arity > 0) == (i->a != SIZE_MAX) && (arity > 1) == (i->b != SIZE_MAX);
}
int mulib_peephole_apply (mulib_mir_native_instruction_t *i, const mulib_peephole_facts_t *f,
    const mulib_peephole_rule_t *rules, size_t count, size_t *matched)
{
  if (!i || !f || !mulib_peephole_instruction_ (i)
      || (f->known >> mulib_mir_native_arity (i->opcode))
      || !mulib_peephole_validate (rules, count)) return -1;
  if (f->left && (f->left == i || !mulib_peephole_binary_ (f->left->opcode)
      || !mulib_peephole_instruction_ (f->left) || f->left->destination != i->a)) return -1;
  if (f->right && (f->right == i || !mulib_peephole_binary_ (f->right->opcode)
      || !mulib_peephole_instruction_ (f->right) || f->right->destination != i->b)) return -1;
  for (size_t k = 0; k < count; ++k) {
    const mulib_peephole_rule_t *r = &rules[k];
    mulib_mir_native_instruction_t out;
    uint64_t value = 0;
    int match = 0;
    if (r->opcode != i->opcode) continue;
    switch (r->guard) {
    case MULIB_PEEPHOLE_FOLD:
      if (i->opcode == MULIB_MIR_NATIVE_COPY) { match = (f->known & 1) != 0; value = f->values[0]; }
      else match = f->known == 3 && mulib_lowopt_evaluate (i->opcode, f->values[0], f->values[1], &value);
      break;
    case MULIB_PEEPHOLE_A: match = (f->known & 1) && f->values[0] == r->literal; break;
    case MULIB_PEEPHOLE_B: match = (f->known & 2) && f->values[1] == r->literal; break;
    case MULIB_PEEPHOLE_SAME: match = i->a == i->b; break;
    case MULIB_PEEPHOLE_SHIFT_ZERO: match = (f->known & 2) && !(f->values[1] & 63); break;
    case MULIB_PEEPHOLE_SELF_COPY: match = i->destination == i->a; break;
    case MULIB_PEEPHOLE_BRANCH_SAME: match = i->target == i->b; break;
    case MULIB_PEEPHOLE_BRANCH_TRUE: match = (f->known & 1) && f->values[0] != 0; break;
    case MULIB_PEEPHOLE_BRANCH_FALSE: match = (f->known & 1) && f->values[0] == 0; break;
    case MULIB_PEEPHOLE_LEFT_A: case MULIB_PEEPHOLE_LEFT_B:
      match = f->left && f->left->opcode == r->nested
          && i->b == (r->guard == MULIB_PEEPHOLE_LEFT_A ? f->left->a : f->left->b); break;
    case MULIB_PEEPHOLE_RIGHT_A: case MULIB_PEEPHOLE_RIGHT_B:
      match = f->right && f->right->opcode == r->nested
          && i->a == (r->guard == MULIB_PEEPHOLE_RIGHT_A ? f->right->a : f->right->b); break;
    default: return -1;
    }
    if (!match) continue;
    out = mulib_mir_native_instruction (MULIB_MIR_NATIVE_CONST); out.destination = i->destination;
    switch (r->action) {
    case MULIB_PEEPHOLE_EVAL: out.immediate = value; break;
    case MULIB_PEEPHOLE_ZERO: break;
    case MULIB_PEEPHOLE_ONE: out.immediate = 1; break;
    case MULIB_PEEPHOLE_ALL: out.immediate = UINT64_MAX; break;
    case MULIB_PEEPHOLE_COPY_A: case MULIB_PEEPHOLE_COPY_B:
      out.opcode = MULIB_MIR_NATIVE_COPY; out.a = r->action == MULIB_PEEPHOLE_COPY_A ? i->a : i->b; break;
    case MULIB_PEEPHOLE_DOUBLE_A: case MULIB_PEEPHOLE_DOUBLE_B:
      out.opcode = MULIB_MIR_NATIVE_ADD; out.a = out.b = r->action == MULIB_PEEPHOLE_DOUBLE_A ? i->a : i->b; break;
    case MULIB_PEEPHOLE_NOP: out = mulib_mir_native_instruction (MULIB_MIR_NATIVE_NOP); break;
    case MULIB_PEEPHOLE_YES: case MULIB_PEEPHOLE_NO:
      out = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP);
      out.target = r->action == MULIB_PEEPHOLE_YES ? i->target : i->b; break;
    case MULIB_PEEPHOLE_LEFT_OTHER:
      if (!f->left) return -1;
      out.opcode = MULIB_MIR_NATIVE_COPY;
      out.a = r->guard == MULIB_PEEPHOLE_LEFT_A ? f->left->b : f->left->a; break;
    case MULIB_PEEPHOLE_RIGHT_OTHER:
      if (!f->right) return -1;
      out.opcode = MULIB_MIR_NATIVE_COPY;
      out.a = r->guard == MULIB_PEEPHOLE_RIGHT_A ? f->right->b : f->right->a; break;
    default: return -1;
    }
    *i = out; if (matched) *matched = k; return 1;
  }
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif
