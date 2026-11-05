#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t calls, fail_at, live;
static void *checked_malloc (size_t n)
{ void *p; if (++calls == fail_at) return NULL; p = malloc (n); if (p) ++live; return p; }
static void *checked_calloc (size_t n, size_t size)
{ void *p; assert (!size || n <= SIZE_MAX / size); p = checked_malloc (n * size); if (p) memset (p,0,n*size); return p; }
static void *checked_realloc (void *old, size_t n)
{ void *p; int had = old != NULL; assert (n); if (++calls == fail_at) return NULL; p = realloc (old,n); if (p && !had) ++live; return p; }
static void checked_free (void *p)
{ if (p) { assert (live); --live; } free (p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LIR
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_ISEL
#define MULIB_IMPL_PEEPHOLE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SUPERINSTR
#define MULIB_IMPL_OPTML
#include "mulib-optml.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
int main (void)
{
  const mulib_mir_native_instruction_t code[] = {
    {MULIB_MIR_NATIVE_PARAM,0,0,SIZE_MAX,SIZE_MAX,0},
    {MULIB_MIR_NATIVE_CONST,1,SIZE_MAX,SIZE_MAX,SIZE_MAX,2},
    {MULIB_MIR_NATIVE_MUL,2,0,1,SIZE_MAX,0},
    {MULIB_MIR_NATIVE_CALL,3,0,1,0,0},
    {MULIB_MIR_NATIVE_ADD,3,2,3,SIZE_MAX,0},
    {MULIB_MIR_NATIVE_RETURN,SIZE_MAX,3,SIZE_MAX,SIZE_MAX,0}
  };
  const size_t arguments[] = {2};
  const char *const symbols[] = {"external_function"};
  mulib_mir_native_block_t block = {0,6};
  mulib_mir_native_program_t p = {code,6,&block,1,4,1,arguments,1,symbols,1};
  int success = 0;
  size_t k;
  for (k = 1; k < 2000; ++k) {
    mulib_optml_result_t r = {0};
    mulib_mir_diagnostic_t d;
    mulib_mir_status_t s;
    calls = 0; fail_at = k;
    s = mulib_optml_compile (&p,"allocation_check",NULL,&r,&d);
    if (s == MULIB_MIR_OK) success = 1;
    else assert (s == MULIB_MIR_NO_MEMORY && d.status == s && !r.assembly && mulib_lir_empty (&r.lir));
    mulib_optml_free (&r); assert (!live);
    if (success) break;
  }
  assert (success && k > 50);
  {
    mulib_muburg_node_t nodes[32];
    const mulib_muburg_rule_t rules[] = {{0,0,0,{0,0},1,1},{0,1,1,{0,0},1,2}};
    mulib_isel_limits_t limits = {32,2,1,32,128,10000};
    memset (nodes,0,sizeof(nodes));
    for (size_t j = 1; j < 32; ++j) { nodes[j].opcode = 1; nodes[j].arity = 1; nodes[j].child[0] = j - 1; }
    success = 0;
    for (k = 1; k < 20; ++k) {
      mulib_isel_cover_t result = {0};
      mulib_mir_status_t s;
      calls = 0; fail_at = k;
      s = mulib_isel_cover (nodes,32,rules,2,1,31,0,limits,&result,NULL);
      if (s == MULIB_MIR_OK) { assert (result.cost == 32); success = 1; }
      else assert (s == MULIB_MIR_NO_MEMORY && !result.memo);
      mulib_isel_cover_free (&result); assert (!live);
      if (success) break;
    }
    assert (success && k >= 4); /* initial cache plus both growth reallocations */
  }
  {
    mulib_optml_rules_t rules = {0};
    calls = 0; fail_at = 1;
    assert (mulib_optml_parse ("fold-add",8,128,&rules,NULL) == MULIB_MIR_NO_MEMORY && !rules.rules && !live);
  }
  {
    static const char body[] = "\tmovq %rax, -80000000(%rbp)\n\tmovq %rcx, -80000008(%rbp)\n"
        "\taddq %rcx, %rax\n\tmovq %rax, -80000016(%rbp)\n";
    char source[sizeof(body) * 8];
    size_t length = 0;
    for (size_t j = 0; j < 8; ++j) { memcpy (source + length,body,sizeof(body)-1); length += sizeof(body)-1; }
    success = 0;
    for (k = 1; k < 100; ++k) {
      mulib_superinstr_result_t result = {0};
      mulib_mir_status_t s;
      calls = 0; fail_at = k;
      s = mulib_superinstr_pack (source,length,"allocation_check",NULL,&result);
      if (s == MULIB_MIR_OK) { assert (result.macros); success = 1; }
      else assert (s == MULIB_MIR_NO_MEMORY && !result.text);
      mulib_superinstr_free (&result); assert (!live);
      if (success) break;
    }
    assert (success);
  }
  {
    mulib_lir_t r = {0};
    size_t baseline;
    fail_at = 0;
    assert (mulib_isel_run (&p,NULL,&r,NULL) == MULIB_MIR_OK);
    baseline = live; success = 0;
    for (k = 1; k < 1000; ++k) {
      mulib_lir_annotation_t *saved = r.annotations;
      mulib_mir_status_t s;
      calls = 0; fail_at = k;
      s = mulib_isel_refresh (&r,NULL,NULL);
      if (s == MULIB_MIR_OK) { success = 1; break; }
      assert (s == MULIB_MIR_NO_MEMORY && r.annotations == saved && live == baseline);
    }
    assert (success); mulib_lir_free (&r); assert (!live);
  }
  return 0;
}
