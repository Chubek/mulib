/* MuLIB compiler tools: Owned token-span syntax tree with parent-child links and subtree traversal.
 * ISO C11 single-header module. Define MULIB_IMPL_SYNTAX_TREE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SYNTAX_TREE_H
#define MULIB_SYNTAX_TREE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t parent,first_child,last_child,next_sibling,start,length;
unsigned kind;
} mulib_syntax_node_t;
typedef struct {mulib_syntax_node_t *nodes;
size_t count,capacity;
} mulib_syntax_tree_t;
int mulib_syntax_tree_add(mulib_syntax_tree_t *t,size_t parent,unsigned kind,size_t start,size_t length,size_t *id);
int mulib_syntax_tree_validate(const mulib_syntax_tree_t *t);
void mulib_syntax_tree_free(mulib_syntax_tree_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYNTAX_TREE_H */
#if defined(MULIB_IMPL_SYNTAX_TREE) && !defined(MULIB_SYNTAX_TREE_IMPLEMENTED)
#define MULIB_SYNTAX_TREE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_syntax_tree_add(mulib_syntax_tree_t *t,size_t parent,unsigned kind,size_t start,size_t len,size_t *id){
 if(!t||!id||start>SIZE_MAX-len||(parent!=SIZE_MAX&&parent>=t->count))return 0;
 if(parent!=SIZE_MAX){const mulib_syntax_node_t *p=t->nodes+parent;
 if(start<p->start||start-p->start>p->length||len>p->length-(start-p->start))return 0;
 }
 if(t->count==t->capacity){size_t cap=t->capacity?t->capacity*2:16;
if(cap<t->capacity||cap>SIZE_MAX/sizeof(*t->nodes))return 0;
void *p=realloc(t->nodes,cap*sizeof(*t->nodes));
if(!p)return 0;
t->nodes=(mulib_syntax_node_t*)p;
t->capacity=cap;
}
 size_t k=t->count++;
t->nodes[k]=(mulib_syntax_node_t){parent,SIZE_MAX,SIZE_MAX,SIZE_MAX,start,len,kind};
 if(parent!=SIZE_MAX){mulib_syntax_node_t *p=&t->nodes[parent];
if(p->last_child==SIZE_MAX)p->first_child=k;
else t->nodes[p->last_child].next_sibling=k;
p->last_child=k;
}
 *id=k;
return 1;
}
int mulib_syntax_tree_validate(const mulib_syntax_tree_t *t){if(!t||(!t->nodes&&t->count)||t->count>t->capacity)return 0;
size_t linked=0,children=0;
for(size_t i=0;i<t->count;i++){const mulib_syntax_node_t *n=&t->nodes[i];
if(n->start>SIZE_MAX-n->length||(n->parent!=SIZE_MAX&&n->parent>=i))return 0;
 if(n->parent!=SIZE_MAX){const mulib_syntax_node_t *p=&t->nodes[n->parent];
children++;
if(n->start<p->start||n->start+n->length>p->start+p->length)return 0;
}else if(n->next_sibling!=SIZE_MAX)return 0;
if((n->first_child==SIZE_MAX)!=(n->last_child==SIZE_MAX))return 0;
size_t previous=i,last=SIZE_MAX;
for(size_t child=n->first_child;child!=SIZE_MAX;child=t->nodes[child].next_sibling){
 if(child<=previous||child>=t->count||t->nodes[child].parent!=i)return 0;
 previous=last=child;
 linked++;
}
if(last!=n->last_child)return 0;
}return linked==children;
}
void mulib_syntax_tree_free(mulib_syntax_tree_t *t){if(t){free(t->nodes);
memset(t,0,sizeof(*t));
}}
#ifdef __cplusplus
}
#endif
#endif
