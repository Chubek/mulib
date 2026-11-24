/* MuLIB compiler tools: Patched balanced AST event-stream iteration with depth budgeting.
 * ISO C11 single-header module. Define MULIB_IMPL_EVTSTREAM in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_EVTSTREAM_H
#define MULIB_EVTSTREAM_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-ast.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-ast.h"
typedef enum mulib_event_kind {MULIB_EVENT_ENTER,MULIB_EVENT_LEAVE} mulib_event_kind_t;
typedef struct mulib_event {mulib_event_kind_t kind;
const mulib_ast_t *node;
size_t depth;
} mulib_event_t;
typedef struct mulib_evtstream {const mulib_ast_t *root,*node;
size_t depth;
int leaving;
} mulib_evtstream_t;
mulib_evtstream_t mulib_evtstream_from_ast(const mulib_ast_t *root);
int mulib_evtstream_next(mulib_evtstream_t *stream,mulib_event_t *event);
/* -2 when traversal would exceed maximum depth; state unchanged. */
int mulib_evtstream_next_bounded(mulib_evtstream_t *stream,mulib_event_t *event,size_t maximum_depth);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EVTSTREAM_H */
#if defined(MULIB_IMPL_EVTSTREAM) && !defined(MULIB_EVTSTREAM_IMPLEMENTED)
#define MULIB_EVTSTREAM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_evtstream_t mulib_evtstream_from_ast(const mulib_ast_t *root){mulib_evtstream_t s={root,root,0,0};
return s;
}
int mulib_evtstream_next_bounded(mulib_evtstream_t *s,mulib_event_t *out,size_t max_depth){
 if(!s||!out)return -1;
if(!s->node)return 0;
if(s->depth>max_depth)return -2;
 if(!s->leaving&&s->node->child&&(s->depth==max_depth||s->depth==SIZE_MAX))return -2;
 mulib_event_t e={s->leaving?MULIB_EVENT_LEAVE:MULIB_EVENT_ENTER,s->node,s->depth};
mulib_evtstream_t next=*s;
 if(!next.leaving){if(next.node->child){next.node=next.node->child;
next.depth++;
}else next.leaving=1;
}
 else if(next.node==next.root)next.node=NULL;
 else if(next.node->next){next.node=next.node->next;
next.leaving=0;
}
 else {if(!next.node->parent||!next.depth)return -1;
next.node=next.node->parent;
next.depth--;
}
 *out=e;
*s=next;
return 1;
}
int mulib_evtstream_next(mulib_evtstream_t *s,mulib_event_t *e){int r=mulib_evtstream_next_bounded(s,e,SIZE_MAX);
return r==-2?-1:r;
}
#ifdef __cplusplus
}
#endif
#endif
