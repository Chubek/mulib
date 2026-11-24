/* MuLIB: formal-aig-cnf. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_AIG_CNF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_AIG_CNF_H
#define MULIB_FORMAL_AIG_CNF_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int *literals;size_t n,cap;size_t *offsets,clauses,offset_cap;unsigned variables;} mulib_formal_cnf_formula;
/* DIMACS variable 1 is forced true; AIG constant zero maps to -1. */
static inline int mulib_formal_aig_cnf_lit(size_t lit){if(lit<2)return lit?1:-1;int v=(int)(lit/2)+1;return lit&1?-v:v;}
static inline int mulib_formal_cnf_clause(mulib_formal_cnf_formula *c,const int *l,size_t n){if(n>SIZE_MAX-c->n||c->clauses>SIZE_MAX-2||!mulib_storage_reserve((void**)&c->offsets,&c->offset_cap,c->clauses+2,sizeof *c->offsets)||!mulib_storage_reserve((void**)&c->literals,&c->cap,c->n+n,sizeof *c->literals))return 0;if(n)memcpy(c->literals+c->n,l,n*sizeof(int));c->offsets[c->clauses++]=c->n;c->n+=n;c->offsets[c->clauses]=c->n;return 1;}
int mulib_formal_aig_cnf(const mulib_synth_aig_graph *a,mulib_formal_cnf_formula *out);
void mulib_formal_cnf_free(mulib_formal_cnf_formula *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_AIG_CNF) && !defined(MULIB_FORMAL_AIG_CNF_H_IMPLEMENTED)
#define MULIB_FORMAL_AIG_CNF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_aig_cnf(const mulib_synth_aig_graph *a,mulib_formal_cnf_formula *out){if(!a||!out||out->literals||out->offsets||a->n>=INT_MAX-1)return 0;mulib_formal_cnf_formula c={0};c.variables=(unsigned)a->n+1;int force=1;if(!mulib_formal_cnf_clause(&c,&force,1))goto fail;for(size_t i=1;i<=a->n;i++){mulib_synth_aig_graph_node n=a->v[i-1];if(n.input)continue;if(n.a/2>=i||n.b/2>=i)goto fail;int x=mulib_formal_aig_cnf_lit(n.a),y=mulib_formal_aig_cnf_lit(n.b),z=(int)i+1;int p[3]={-z,x,0},q[3]={-z,y,0},r[3]={z,-x,-y};if(!mulib_formal_cnf_clause(&c,p,2)||!mulib_formal_cnf_clause(&c,q,2)||!mulib_formal_cnf_clause(&c,r,3))goto fail;}*out=c;return 1;fail:free(c.literals);free(c.offsets);return 0;}
void mulib_formal_cnf_free(mulib_formal_cnf_formula *c){if(c){free(c->literals);free(c->offsets);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
