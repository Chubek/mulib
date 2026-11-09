/* Independent integer Floyd-Warshall oracle over all Boolean assignments. */
#define MULIB_IMPL_SAT
#define MULIB_IMPL_SMT
#include "mulib-smt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t state=0x513a6789;
static uint32_t random_word(void) {
  state^=state<<13;state^=state>>17;state^=state<<5;return state;
}
static int enabled(unsigned mask,int guard) {
  unsigned variable=(unsigned)(guard<0?-guard:guard);
  return !guard || ((mask>>(variable-1))&1U)==(unsigned)(guard>0);
}
int main(void) {
  size_t test;
  for(test=0;test<500;++test) {
    int left[12],right[12],bound[12],guard[12],clauses[4][3];
    size_t count=random_word()%13,nclauses=random_word()%5,i,j,k;
    unsigned mask;int expected=0;
    mulib_smt_t *solver=mulib_smt_create(3,4,100);
    assert(solver);
    for(i=0;i<nclauses;++i) {
      for(j=0;j<3;++j)clauses[i][j]=(int)(random_word()%3+1)*(random_word()%2?1:-1);
      assert(mulib_smt_clause(solver,clauses[i],3));
    }
    for(i=0;i<count;++i) {
      left[i]=(int)(random_word()%4);right[i]=(int)(random_word()%4);
      bound[i]=(int)(random_word()%7)-3;guard[i]=(int)(random_word()%7)-3;
      assert(mulib_smt_difference(solver,(size_t)left[i],(size_t)right[i],bound[i],guard[i]));
    }
    for(mask=0;mask<8;++mask) {
      int distance[4][4],feasible=1;
      for(i=0;i<nclauses;++i) {
        int satisfied=0;
        for(j=0;j<3;++j)if(enabled(mask,clauses[i][j]))satisfied=1;
        if(!satisfied)feasible=0;
      }
      if(!feasible)continue;
      for(i=0;i<4;++i)for(j=0;j<4;++j)distance[i][j]=i==j?0:1000000;
      for(i=0;i<count;++i)if(enabled(mask,guard[i])&&bound[i]<distance[right[i]][left[i]])
        distance[right[i]][left[i]]=bound[i];
      for(k=0;k<4;++k)for(i=0;i<4;++i)for(j=0;j<4;++j)
        if(distance[i][k]<1000000&&distance[k][j]<1000000&&
           distance[i][j]>distance[i][k]+distance[k][j])
          distance[i][j]=distance[i][k]+distance[k][j];
      for(i=0;i<4;++i)if(distance[i][i]<0)feasible=0;
      if(feasible){expected=1;break;}
    }
    for(k=0;k<2;++k) { /* repeated solves must preserve satisfiability */
      assert(mulib_smt_solve(solver)==expected);
      if(expected) {
        double values[4];unsigned assignment=0;
        for(i=0;i<3;++i)if(mulib_smt_boolean_value(solver,i+1)>0)assignment|=1U<<i;
        for(i=0;i<4;++i)assert(mulib_smt_real_value(solver,i,&values[i]));
        for(i=0;i<count;++i)if(enabled(assignment,guard[i]))
          assert(values[left[i]]-values[right[i]]<=bound[i]);
        for(i=0;i<nclauses;++i)
          assert(enabled(assignment,clauses[i][0])||enabled(assignment,clauses[i][1])||enabled(assignment,clauses[i][2]));
      }
    }
    mulib_smt_destroy(solver);
  }
  puts("smt exhaustive Boolean/integer-theory oracle passed");
  return 0;
}
