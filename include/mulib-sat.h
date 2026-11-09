/** @file mulib-sat.h
 * @brief Bounded exact CNF SAT using DPLL, deterministic branching, unit
 * propagation, assumptions and model retrieval. Not a CDCL solver.
 * Define MULIB_IMPL_SAT in one C11 translation unit. No external libraries.
 */
#ifndef MULIB_SAT_H
#define MULIB_SAT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sat mulib_sat_t;
/** SAT=1, UNSAT=0, invalid=-1, search limit=-2, OOM=-3. */
typedef enum mulib_sat_status {
  MULIB_SAT_SAT=1, MULIB_SAT_UNSAT=0, MULIB_SAT_INVALID=-1,
  MULIB_SAT_LIMIT=-2, MULIB_SAT_MEMORY=-3
} mulib_sat_status_t;
/** Variables numbered 1..variables; variables<=4096; 0 permitted.
 * max_decisions=0 selects 1000000 recursive decision nodes. */
mulib_sat_t *mulib_sat_create(size_t variables, size_t max_decisions);
void mulib_sat_destroy(mulib_sat_t *solver);
/** Add a clause of signed literals +/-[1,variables]. Empty clause is false;
 * no modification on invalid input/OOM. Returns 1 on success. */
int mulib_sat_add_clause(mulib_sat_t *solver, const int *literals, size_t n);
/** Solve with optional temporary unit assumptions. A SAT model remains valid
 * until next mutation/solve. Returns mulib_sat_status_t. */
int mulib_sat_solve(mulib_sat_t *solver, const int *assumptions, size_t n);
/** Get model value (+1/-1). Returns 0 if no SAT model or invalid variable. */
int mulib_sat_value(const mulib_sat_t *solver, size_t variable);
size_t mulib_sat_variables(const mulib_sat_t *solver);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SAT) && !defined(MULIB_SAT_IMPLEMENTED)
#define MULIB_SAT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sat_clause_ {int *lit;size_t n;} mulib_sat_clause_;
struct mulib_sat {
  size_t variables, count, capacity, max_decisions;
  mulib_sat_clause_ *clauses;
  signed char *model;
  int has_model;
};
typedef struct mulib_sat_context_ {
  const mulib_sat_t *s; signed char *a; size_t *trail;
  size_t used, nodes;
} mulib_sat_context_;
mulib_sat_t *mulib_sat_create(size_t variables,size_t max_decisions) {
  mulib_sat_t *s;
  if(variables>4096)return NULL;
  s=(mulib_sat_t *)calloc(1,sizeof(*s));if(!s)return NULL;
  s->model=(signed char *)calloc(variables+1,sizeof(*s->model));
  if(!s->model){free(s);return NULL;}
  s->variables=variables;s->max_decisions=max_decisions?max_decisions:1000000;
  return s;
}
void mulib_sat_destroy(mulib_sat_t *s) {
  size_t i;if(!s)return;
  for(i=0;i<s->count;++i)free(s->clauses[i].lit);
  free(s->clauses);free(s->model);free(s);
}
size_t mulib_sat_variables(const mulib_sat_t *s){return s?s->variables:0;}
int mulib_sat_add_clause(mulib_sat_t *s,const int *lit,size_t n) {
  size_t i;int *p=NULL;mulib_sat_clause_ *newc;
  if(!s||(!lit&&n)||n>SIZE_MAX/sizeof(int))return 0;
  for(i=0;i<n;++i) {
    if(lit[i]==0||(long long)lit[i]>(long long)s->variables||
       (long long)lit[i]<-(long long)s->variables)return 0;
  }
  if(n) {p=(int *)malloc(n*sizeof(int));if(!p)return 0;memcpy(p,lit,n*sizeof(int));}
  if(s->count==s->capacity){
    size_t cap=s->capacity?s->capacity*2:8;
    if(cap<s->capacity||cap>SIZE_MAX/sizeof(*newc)){free(p);return 0;}
    newc=(mulib_sat_clause_ *)realloc(s->clauses,cap*sizeof(*newc));
    if(!newc){free(p);return 0;}s->clauses=newc;s->capacity=cap;
  }
  s->clauses[s->count].lit=p;s->clauses[s->count].n=n;
  ++s->count;s->has_model=0;return 1;
}
static int mulib_sat_set_(mulib_sat_context_ *c,int lit) {
  size_t v=(size_t)(lit<0?-(long long)lit:lit);
  signed char desired=(signed char)(lit<0?-1:1);
  if(c->a[v])return c->a[v]==desired;
  c->a[v]=desired;c->trail[c->used++]=v;return 1;
}
static void mulib_sat_undo_(mulib_sat_context_ *c,size_t mark) {
  while(c->used>mark)c->a[c->trail[--c->used]]=0;
}
static int mulib_sat_propagate_(mulib_sat_context_ *c) {
  int changed;size_t i,j;
  do {changed=0;
    for(i=0;i<c->s->count;++i){
      const mulib_sat_clause_ *cl=&c->s->clauses[i];
      int unit=0,satisfied=0;size_t unknown=0;
      for(j=0;j<cl->n;++j){int lit=cl->lit[j];
        size_t v=(size_t)(lit<0?-(long long)lit:lit);
        if(c->a[v]==0){++unknown;unit=lit;}
        else if((lit>0&&c->a[v]>0)||(lit<0&&c->a[v]<0)){satisfied=1;break;}
      }
      if(satisfied)continue;
      if(!unknown)return 0;
      if(unknown==1){if(!mulib_sat_set_(c,unit))return 0;changed=1;}
    }
  }while(changed);
  return 1;
}
static int mulib_sat_search_(mulib_sat_context_ *c) {
  size_t start=c->used,base,i,j,variable=0;int r;
  if(!mulib_sat_propagate_(c)){mulib_sat_undo_(c,start);return MULIB_SAT_UNSAT;}
  base=c->used;
  /* Do not spend decisions (or stack frames) assigning unconstrained variables. */
  for(i=0;i<c->s->count;++i) {
    const mulib_sat_clause_ *cl=&c->s->clauses[i];
    size_t candidate=0;int satisfied=0;
    for(j=0;j<cl->n;++j) {
      int lit=cl->lit[j];size_t v=(size_t)(lit<0?-lit:lit);
      if((lit>0&&c->a[v]>0)||(lit<0&&c->a[v]<0)){satisfied=1;break;}
      if(!c->a[v]&&(!candidate||v<candidate))candidate=v;
    }
    if(!satisfied&&candidate&&(!variable||candidate<variable))variable=candidate;
  }
  if(!variable)return MULIB_SAT_SAT;
  if(c->nodes>=c->s->max_decisions){mulib_sat_undo_(c,start);return MULIB_SAT_LIMIT;}
  ++c->nodes;
  (void)mulib_sat_set_(c,(int)variable);r=mulib_sat_search_(c);
  if(r==MULIB_SAT_SAT)return r;
  mulib_sat_undo_(c,base);
  if(r==MULIB_SAT_LIMIT){mulib_sat_undo_(c,start);return r;}
  (void)mulib_sat_set_(c,-(int)variable);r=mulib_sat_search_(c);
  if(r==MULIB_SAT_SAT)return r;
  mulib_sat_undo_(c,start);return r;
}
int mulib_sat_solve(mulib_sat_t *s,const int *assumptions,size_t n) {
  mulib_sat_context_ c;size_t i;int r;
  if(!s)return MULIB_SAT_INVALID;
  s->has_model=0;
  if(!assumptions&&n)return MULIB_SAT_INVALID;
  /* Validate every literal before interpreting contradictory assumptions. */
  for(i=0;i<n;++i)if(!assumptions[i]||
      (long long)assumptions[i]>(long long)s->variables||
      (long long)assumptions[i]<-(long long)s->variables)return MULIB_SAT_INVALID;
  c.s=s;c.used=0;c.nodes=0;
  c.a=(signed char *)calloc(s->variables+1,sizeof(*c.a));
  c.trail=(size_t *)malloc((s->variables+1)*sizeof(*c.trail));
  if(!c.a||!c.trail){free(c.a);free(c.trail);return MULIB_SAT_MEMORY;}
  for(i=0;i<n;++i){int lit=assumptions[i];
    if(!mulib_sat_set_(&c,lit)){r=MULIB_SAT_UNSAT;goto done;}
  }
  r=mulib_sat_search_(&c);
  if(r==MULIB_SAT_SAT){
    for(i=1;i<=s->variables;++i)s->model[i]=c.a[i]?c.a[i]:-1;
    s->has_model=1;
  }
done:
  free(c.a);free(c.trail);return r;
}
int mulib_sat_value(const mulib_sat_t *s,size_t v) {
  return s&&s->has_model&&v>0&&v<=s->variables?s->model[v]:0;
}
#ifdef __cplusplus
}
#endif
#endif
