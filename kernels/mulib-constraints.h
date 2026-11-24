/* MuLIB compiler tools: Union-find unification constraints with optional concrete type assignments.
 * ISO C11 single-header module. Define MULIB_IMPL_CONSTRAINTS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_CONSTRAINTS_H
#define MULIB_CONSTRAINTS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t parent,rank,type;
} mulib_constraint_var_t;
typedef struct {mulib_constraint_var_t *vars;
size_t count,capacity;
} mulib_constraints_t;
int mulib_constraints_new(mulib_constraints_t *c,size_t *id);
int mulib_constraints_unify(mulib_constraints_t *c,size_t a,size_t b);
int mulib_constraints_bind(mulib_constraints_t *c,size_t var,size_t type);
int mulib_constraints_type(mulib_constraints_t *c,size_t var,size_t *type);
void mulib_constraints_free(mulib_constraints_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CONSTRAINTS_H */
#if defined(MULIB_IMPL_CONSTRAINTS) && !defined(MULIB_CONSTRAINTS_IMPLEMENTED)
#define MULIB_CONSTRAINTS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t mulib_constraints_find_(mulib_constraints_t *c,size_t id){size_t p=id;
while(c->vars[p].parent!=p)p=c->vars[p].parent;
while(c->vars[id].parent!=id){size_t next=c->vars[id].parent;
c->vars[id].parent=p;
id=next;
}return p;
}
int mulib_constraints_new(mulib_constraints_t *c,size_t *id){if(!c||!id)return 0;
if(c->count==c->capacity){size_t cap=c->capacity?c->capacity*2:8;
if(cap<c->capacity||cap>SIZE_MAX/sizeof(*c->vars))return 0;
void *p=realloc(c->vars,cap*sizeof(*c->vars));
if(!p)return 0;
c->vars=(mulib_constraint_var_t*)p;
c->capacity=cap;
}*id=c->count;
c->vars[c->count]=(mulib_constraint_var_t){c->count,0,SIZE_MAX};
c->count++;
return 1;
}
int mulib_constraints_unify(mulib_constraints_t *c,size_t a,size_t b){if(!c||a>=c->count||b>=c->count)return 0;
a=mulib_constraints_find_(c,a);
b=mulib_constraints_find_(c,b);
if(a==b)return 1;
if(c->vars[a].type!=SIZE_MAX&&c->vars[b].type!=SIZE_MAX&&c->vars[a].type!=c->vars[b].type)return 0;
if(c->vars[a].rank<c->vars[b].rank){size_t x=a;
a=b;
b=x;
}c->vars[b].parent=a;
if(c->vars[a].rank==c->vars[b].rank)c->vars[a].rank++;
if(c->vars[a].type==SIZE_MAX)c->vars[a].type=c->vars[b].type;
return 1;
}
int mulib_constraints_bind(mulib_constraints_t *c,size_t x,size_t ty){if(!c||x>=c->count||ty==SIZE_MAX)return 0;
x=mulib_constraints_find_(c,x);
if(c->vars[x].type!=SIZE_MAX&&c->vars[x].type!=ty)return 0;
c->vars[x].type=ty;
return 1;
}
int mulib_constraints_type(mulib_constraints_t *c,size_t x,size_t *ty){if(!c||x>=c->count||!ty)return 0;
x=mulib_constraints_find_(c,x);
if(c->vars[x].type==SIZE_MAX)return 0;
*ty=c->vars[x].type;
return 1;
}
void mulib_constraints_free(mulib_constraints_t *c){if(c){free(c->vars);
memset(c,0,sizeof(*c));
}}
#ifdef __cplusplus
}
#endif
#endif
