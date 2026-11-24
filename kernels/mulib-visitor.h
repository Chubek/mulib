/* MuLIB compiler tools: Preorder AST visitor with bounded traversal.
 * ISO C11 single-header module. Define MULIB_IMPL_VISITOR in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_VISITOR_H
#define MULIB_VISITOR_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-ast.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-ast.h"
typedef enum mulib_visit_result {MULIB_VISIT_STOP,MULIB_VISIT_DESCEND,MULIB_VISIT_SKIP} mulib_visit_result_t;
typedef mulib_visit_result_t (*mulib_visitor_fn)(const mulib_ast_t *,void *);
/* Original API, 1 complete, 0 stopped, -1 invalid. */
int mulib_visitor_walk(const mulib_ast_t *root,mulib_visitor_fn visit,void *user);
/* Additional API: -2 when node budget is exhausted. Prior callbacks are retained.
 * Trees are borrowed, valid ASTs and must not be mutated during traversal. */
int mulib_visitor_walk_bounded(const mulib_ast_t *root,mulib_visitor_fn visit,void *user,size_t max_nodes);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VISITOR_H */
#if defined(MULIB_IMPL_VISITOR) && !defined(MULIB_VISITOR_IMPLEMENTED)
#define MULIB_VISITOR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_visitor_walk_bounded(const mulib_ast_t *root,mulib_visitor_fn f,void *u,size_t limit){
 if(!root||!f)return -1;
const mulib_ast_t *n=root;
size_t work=0;
 for(;;){if(work>=limit)return -2;
work++;
mulib_visit_result_t r=f(n,u);
 if(r==MULIB_VISIT_STOP)return 0;
if(r!=MULIB_VISIT_DESCEND&&r!=MULIB_VISIT_SKIP)return -1;
 if(r==MULIB_VISIT_DESCEND&&n->child){n=n->child;
continue;
}
 while(n!=root&&!n->next){if(!n->parent)return -1;
n=n->parent;
}
 if(n==root)return 1;
n=n->next;
}
}
int mulib_visitor_walk(const mulib_ast_t *root,mulib_visitor_fn f,void *user){int r=mulib_visitor_walk_bounded(root,f,user,SIZE_MAX);
return r==-2?-1:r;
}
#ifdef __cplusplus
}
#endif
#endif
