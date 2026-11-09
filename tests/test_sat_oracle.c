/* Compare DPLL with exhaustive truth-table oracle for 400 random 4-SAT cases. */
#define MULIB_IMPL_SAT
#include "mulib-sat.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint32_t rng=0x10937;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
  size_t test;
  for(test=0;test<400;++test){
    int cnf[12][4],lens[12];size_t nvars=2+rnd()%5,nclauses=rnd()%13,mask,j,k;
    mulib_sat_t *s=mulib_sat_create(nvars,0);int expected=0,actual;
    assert(s);
    for(j=0;j<nclauses;++j){
      lens[j]=(int)(rnd()%5);
      for(k=0;k<(size_t)lens[j];++k){int v=1+(int)(rnd()%nvars);
        cnf[j][k]=(rnd()%2)?v:-v;
      }
      assert(mulib_sat_add_clause(s,cnf[j],(size_t)lens[j]));
    }
    for(mask=0;mask<((size_t)1<<nvars);++mask){int all=1;
      for(j=0;j<nclauses;++j){int satisfied=0;
        for(k=0;k<(size_t)lens[j];++k){
          int lit=cnf[j][k];size_t var=(size_t)(lit>0?lit:-lit);
          if(((mask>>(var-1))&1U)==(unsigned)(lit>0)){satisfied=1;break;}
        }
        if(!satisfied){all=0;break;}
      }
      if(all){expected=1;break;}
    }
    actual=mulib_sat_solve(s,NULL,0);
    assert(actual==expected);
    if(actual==MULIB_SAT_SAT){
      for(j=0;j<nclauses;++j){int satisfied=0;
        for(k=0;k<(size_t)lens[j];++k){
          int lit=cnf[j][k],value=mulib_sat_value(s,(size_t)(lit>0?lit:-lit));
          if((lit>0&&value==1)||(lit<0&&value==-1)){satisfied=1;break;}
        }
        assert(satisfied);
      }
    }
    mulib_sat_destroy(s);
  }
  puts("sat exhaustive-oracle properties passed");return 0;
}
