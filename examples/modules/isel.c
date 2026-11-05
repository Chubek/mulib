#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LIR
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_ISEL
#include "mulib-isel.h"
#include <assert.h>
int main (void)
{
  mulib_muburg_node_t nodes[] = {{0,0,{0,0}}, {1,2,{0,0}}};
  mulib_muburg_rule_t rules[] = {{0,0,0,{0,0},1,1}, {0,1,2,{0,0},2,2}};
  mulib_isel_limits_t limits = {2,2,1,2,8,100};
  mulib_isel_cover_t cover = {0};
  assert (mulib_isel_cover (nodes,2,rules,2,1,1,0,limits,&cover,NULL) == MULIB_MIR_OK);
  assert (cover.cost == 4 && cover.count == 2);
  mulib_isel_cover_free (&cover); return 0;
}
