#define MULIB_IMPL_MIR
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_PEEPHOLE
#include "mulib-peephole.h"
#include <assert.h>
int main (void)
{
  mulib_mir_native_instruction_t i = {MULIB_MIR_NATIVE_ADD,2,0,1,SIZE_MAX,0};
  mulib_peephole_facts_t facts = {2,{0,0},NULL,NULL};
  size_t count, matched;
  const mulib_peephole_rule_t *rules = mulib_peephole_rules (&count);
  assert (count > 60);
  assert (mulib_peephole_apply (&i,&facts,rules,count,&matched) == 1);
  assert (i.opcode == MULIB_MIR_NATIVE_COPY && i.a == 0);
  return 0;
}
