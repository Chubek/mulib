/* MuLIB compiler tools: Patched production-specific AST listeners with explicit event budget.
 * ISO C11 single-header module. Define MULIB_IMPL_LISTENER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LISTENER_H
#define MULIB_LISTENER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-evtstream.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-evtstream.h"
typedef struct mulib_listener {const char *production;
mulib_ast_action_fn enter,leave;
void *user;
} mulib_listener_t;
int mulib_listener_walk(const mulib_ast_t *root,const mulib_listener_t *listeners,size_t count);
int mulib_listener_walk_bounded(const mulib_ast_t *root,const mulib_listener_t *listeners,size_t count,size_t max_events);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LISTENER_H */
#if defined(MULIB_IMPL_LISTENER) && !defined(MULIB_LISTENER_IMPLEMENTED)
#define MULIB_LISTENER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_listener_walk_bounded(const mulib_ast_t *root,const mulib_listener_t *ls,size_t n,size_t max){
 if(!root||(!ls&&n))return -1;
mulib_evtstream_t stream=mulib_evtstream_from_ast(root);
mulib_event_t e;
size_t count=0;
int rc;
 while((rc=mulib_evtstream_next(&stream,&e))==1){if(count>=max)return -2;
count++;
for(size_t i=0;i<n;i++){
 mulib_ast_action_fn fn=e.kind==MULIB_EVENT_ENTER?ls[i].enter:ls[i].leave;
 if(fn&&(!ls[i].production||(e.node->production&&!strcmp(ls[i].production,e.node->production)))&&!fn(e.node,ls[i].user))return 0;
}}
 return rc<0?-1:1;
}
int mulib_listener_walk(const mulib_ast_t *root,const mulib_listener_t *ls,size_t n){int r=mulib_listener_walk_bounded(root,ls,n,SIZE_MAX);
return r==-2?-1:r;
}
#ifdef __cplusplus
}
#endif
#endif
