/* MuLIB: formal-transition. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_TRANSITION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_TRANSITION_H
#define MULIB_FORMAL_TRANSITION_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t states,edges;const unsigned char *initial;const size_t *offsets,*targets;} mulib_formal_transition;
int mulib_formal_transition_valid(const mulib_formal_transition *s);
int mulib_formal_transition_reachable(const mulib_formal_transition *s,unsigned char *reachable,size_t *parent,size_t *distance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_TRANSITION) && !defined(MULIB_FORMAL_TRANSITION_H_IMPLEMENTED)
#define MULIB_FORMAL_TRANSITION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_transition_valid(const mulib_formal_transition *s){if(!s||!s->states||!s->initial||!s->offsets||s->offsets[0]||s->offsets[s->states]!=s->edges||(!s->targets&&s->edges))return 0;for(size_t i=0;i<s->states;i++)if(s->offsets[i]>s->offsets[i+1]||s->offsets[i+1]>s->edges)return 0;for(size_t i=0;i<s->edges;i++)if(s->targets[i]>=s->states)return 0;return 1;}
int mulib_formal_transition_reachable(const mulib_formal_transition *s,unsigned char *reachable,size_t *parent,size_t *distance){if(!mulib_formal_transition_valid(s)||!reachable||!parent||!distance)return 0;size_t *q=(size_t*)mulib_storage_alloc(s->states,sizeof *q);if(!q)return 0;size_t head=0,tail=0;for(size_t i=0;i<s->states;i++){reachable[i]=!!s->initial[i];parent[i]=MULIB_STORAGE_NONE;distance[i]=reachable[i]?0:MULIB_STORAGE_NONE;if(reachable[i])q[tail++]=i;}while(head<tail){size_t u=q[head++];for(size_t k=s->offsets[u];k<s->offsets[u+1];k++){size_t v=s->targets[k];if(!reachable[v]){reachable[v]=1;parent[v]=u;distance[v]=distance[u]+1;q[tail++]=v;}}}free(q);return 1;}
#ifdef __cplusplus
}
#endif
#endif
