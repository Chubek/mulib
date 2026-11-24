/* MuLIB: formal-proof. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_PROOF in exactly one implementation translation unit.
 * Profile: Single-clause RUP check; not complete DRAT/LRAT proof processing.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_PROOF_H
#define MULIB_FORMAL_PROOF_H
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
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#include "mulib-formal-aig-cnf.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_proof_rup(const mulib_formal_cnf_formula *c,const int *clause,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_PROOF) && !defined(MULIB_FORMAL_PROOF_H_IMPLEMENTED)
#define MULIB_FORMAL_PROOF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_proof_rup(const mulib_formal_cnf_formula *c,const int *clause,size_t n){if(!c||(!clause&&n)||!c->offsets||c->offsets[0]||c->offsets[c->clauses]!=c->n||(!c->literals&&c->n))return -1;size_t vars=(size_t)c->variables+1;signed char *v=(signed char*)mulib_storage_alloc(vars,1);if(!v)return -1;int result=0;for(size_t i=0;i<n;i++){int lit=clause[i];if(!lit||lit==INT_MIN||(unsigned)abs(lit)>c->variables){result=-1;goto end;}unsigned k=(unsigned)abs(lit);signed char x=lit>0?-1:1;if(v[k]&&v[k]!=x){result=1;goto end;}v[k]=x;}int change=1;while(change){change=0;for(size_t j=0;j<c->clauses;j++){if(c->offsets[j]>c->offsets[j+1]||c->offsets[j+1]>c->n){result=-1;goto end;}size_t unknown=0;int last=0,ok=0;for(size_t i=c->offsets[j];i<c->offsets[j+1];i++){int l=c->literals[i];if(!l||l==INT_MIN||(unsigned)abs(l)>c->variables){result=-1;goto end;}signed char x=v[abs(l)];if(x==(l>0?1:-1)){ok=1;break;}if(!x){unknown++;last=l;}}if(ok)continue;if(!unknown){result=1;goto end;}if(unknown==1){v[abs(last)]=last>0?1:-1;change=1;}}}end:free(v);return result;}
#ifdef __cplusplus
}
#endif
#endif
