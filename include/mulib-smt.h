/** @file mulib-smt.h
 * @brief Bounded SMT for Boolean CNF + floating-point real difference logic
 * (x_i - x_j <= c) via SAT enumeration and Bellman-Ford theory checking.
 * This is NOT general-purpose SMT-LIB, bitvectors, linear arithmetic or an
 * industrial DPLL(T) solver. Define MULIB_IMPL_SAT and MULIB_IMPL_SMT;
 * include this header in one C11 TU. No external dependencies.
 */
#ifndef MULIB_SMT_H
#define MULIB_SMT_H
#include <stddef.h>
#include "mulib-sat.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_smt mulib_smt_t;
/** SAT status semantics; LIMIT also covers unrepresentable theory witnesses. */
typedef enum mulib_smt_status {
  MULIB_SMT_SAT=1, MULIB_SMT_UNSAT=0, MULIB_SMT_INVALID=-1,
  MULIB_SMT_LIMIT=-2, MULIB_SMT_MEMORY=-3
} mulib_smt_status_t;
/** Boolean variables 1..bools, real variables 0..reals-1. */
mulib_smt_t *mulib_smt_create(size_t bools, size_t reals, size_t max_models);
void mulib_smt_destroy(mulib_smt_t *solver);
/** CNF clause, signed Boolean literals. Returns 1 on success. */
int mulib_smt_clause(mulib_smt_t *solver, const int *literals, size_t n);
/** Add x[left] - x[right] <= bound, active iff guard literal holds.
 * guard=0 means always active, otherwise signed Boolean literal. */
int mulib_smt_difference(mulib_smt_t *solver, size_t left, size_t right,
    double bound, int guard);
/** Enumerate up to max_models Boolean assignments, using long-double
 * Bellman-Ford relaxation (not exact rational arithmetic).
 * LIMIT also reports numerical failure; no model is exposed in that case. */
int mulib_smt_solve(mulib_smt_t *solver);
int mulib_smt_boolean_value(const mulib_smt_t *solver, size_t variable);
/** Returns 1 and writes real value only for a current SAT model. */
int mulib_smt_real_value(const mulib_smt_t *solver, size_t variable, double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SMT) && !defined(MULIB_SMT_IMPLEMENTED)
#define MULIB_SMT_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_smt_diff_ {size_t left,right; double bound; int guard;} mulib_smt_diff_;
struct mulib_smt {
  mulib_sat_t *sat;size_t bools,reals,count,capacity,max_models;
  mulib_smt_diff_ *diffs;double *model;int has_model;
};
mulib_smt_t *mulib_smt_create(size_t bools,size_t reals,size_t max_models) {
  mulib_smt_t *s;
  if(bools>4096||reals>4096)return NULL;
  s=(mulib_smt_t *)calloc(1,sizeof(*s));if(!s)return NULL;
  s->sat=mulib_sat_create(bools,0);
  s->model=(double *)calloc(reals?reals:1,sizeof(*s->model));
  if(!s->sat||!s->model){mulib_sat_destroy(s->sat);free(s->model);free(s);return NULL;}
  s->bools=bools;s->reals=reals;s->max_models=max_models?max_models:10000;
  return s;
}
void mulib_smt_destroy(mulib_smt_t *s) {
  if(!s)return;
  mulib_sat_destroy(s->sat);free(s->model);free(s->diffs);free(s);
}
int mulib_smt_clause(mulib_smt_t *s,const int *lit,size_t n) {
  int ok;if(!s)return 0;ok=mulib_sat_add_clause(s->sat,lit,n);
  if(ok)s->has_model=0;
  return ok;
}
int mulib_smt_difference(mulib_smt_t *s,size_t left,size_t right,double bound,int guard) {
  mulib_smt_diff_ *p;
  if(!s||left>=s->reals||right>=s->reals||!isfinite(bound)||
     (long long)guard>(long long)s->bools||
     (long long)guard<-(long long)s->bools)return 0;
  if(s->count==s->capacity){size_t cap=s->capacity?s->capacity*2:8;
    if(cap<s->capacity||cap>SIZE_MAX/sizeof(*p))return 0;
    p=(mulib_smt_diff_ *)realloc(s->diffs,cap*sizeof(*p));
    if(!p)return 0;
    s->diffs=p;s->capacity=cap;
  }
  s->diffs[s->count].left=left;s->diffs[s->count].right=right;
  s->diffs[s->count].bound=bound;s->diffs[s->count].guard=guard;
  ++s->count;s->has_model=0;return 1;
}
static int mulib_smt_enabled_(const mulib_smt_t *s,int guard) {
  return guard==0 || mulib_sat_value(s->sat,(size_t)(guard<0?-(long long)guard:guard))==(guard<0?-1:1);
}
static int mulib_smt_theory_(const mulib_smt_t *s,double *out) {
  long double *distance;size_t i,j;int changed=0,ok=1;
  distance=(long double *)calloc(s->reals?s->reals:1,sizeof(*distance));
  if(!distance)return -1;
  for(i=0;i<s->reals;++i){changed=0;
    for(j=0;j<s->count;++j){const mulib_smt_diff_ *d=&s->diffs[j];long double candidate;
      if(!mulib_smt_enabled_(s,d->guard))continue;
      candidate=distance[d->right]+(long double)d->bound;
      if(distance[d->left]>candidate){distance[d->left]=candidate;changed=1;}
    }
    if(!changed)break;
  }
  if(changed){
    for(j=0;j<s->count;++j){const mulib_smt_diff_ *d=&s->diffs[j];
      if(mulib_smt_enabled_(s,d->guard) &&
         distance[d->left]>distance[d->right]+(long double)d->bound){ok=0;break;}
    }
  }
  if(ok)for(i=0;i<s->reals;++i){out[i]=(double)distance[i];
    if(!isfinite(out[i])){ok=-2;break;}
  }
  /* Rounding distances to double may destroy a feasible long-double witness.
   * Never publish it or permanently block this potentially feasible branch. */
  if(ok==1)for(j=0;j<s->count;++j){const mulib_smt_diff_ *d=&s->diffs[j];
    if(mulib_smt_enabled_(s,d->guard)&&
       (long double)out[d->left]-(long double)out[d->right]>(long double)d->bound){ok=-2;break;}
  }
  free(distance);return ok;
}
int mulib_smt_solve(mulib_smt_t *s) {
  double *scratch;int *block;int status;size_t attempts,i;
  if(!s)return MULIB_SMT_INVALID;
  s->has_model=0;
  scratch=(double *)calloc(s->reals?s->reals:1,sizeof(*scratch));
  block=(int *)malloc((s->bools?s->bools:1)*sizeof(*block));
  if(!scratch||!block){free(scratch);free(block);return MULIB_SMT_MEMORY;}
  status=MULIB_SMT_LIMIT;
  for(attempts=0;attempts<s->max_models;++attempts){
    int sat=mulib_sat_solve(s->sat,NULL,0);
    if(sat!=MULIB_SAT_SAT){status=sat;break;}
    sat=mulib_smt_theory_(s,scratch);
    if(sat==1){memcpy(s->model,scratch,s->reals*sizeof(double));s->has_model=1;status=MULIB_SMT_SAT;break;}
    if(sat<0){status=sat==-1?MULIB_SMT_MEMORY:MULIB_SMT_LIMIT;break;}
    /* Entire Boolean assignment is theory-infeasible, independently of any
       subsequent added constraints: a permanent exact blocking clause is sound. */
    for(i=0;i<s->bools;++i)block[i]=mulib_sat_value(s->sat,i+1)>0?-(int)(i+1):(int)(i+1);
    if(!mulib_sat_add_clause(s->sat,block,s->bools)){status=MULIB_SMT_MEMORY;break;}
  }
  free(scratch);free(block);return status;
}
int mulib_smt_boolean_value(const mulib_smt_t *s,size_t variable) {
  return s&&s->has_model?mulib_sat_value(s->sat,variable):0;
}
int mulib_smt_real_value(const mulib_smt_t *s,size_t variable,double *out) {
  if(!s||!s->has_model||variable>=s->reals||!out)return 0;
  *out=s->model[variable];return 1;
}
#ifdef __cplusplus
}
#endif
#endif
