/* MuLIB compiler tools: Owned nested lexical symbol tables with shadowing, stable IDs, and scope rollback.
 * ISO C11 single-header module. Define MULIB_IMPL_SYMBOLTABLE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SYMBOLTABLE_H
#define MULIB_SYMBOLTABLE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {char *name;
uint64_t value;
size_t scope;
} mulib_symboltable_entry_t;
typedef struct {mulib_symboltable_entry_t *items;
size_t count,capacity;
size_t *scope_starts,depth,scope_capacity;
} mulib_symboltable_t;
int mulib_symboltable_enter(mulib_symboltable_t *t);
int mulib_symboltable_leave(mulib_symboltable_t *t);
int mulib_symboltable_define(mulib_symboltable_t *t,const char *name,uint64_t value);
int mulib_symboltable_lookup(const mulib_symboltable_t *t,const char *name,uint64_t *value);
void mulib_symboltable_free(mulib_symboltable_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYMBOLTABLE_H */
#if defined(MULIB_IMPL_SYMBOLTABLE) && !defined(MULIB_SYMBOLTABLE_IMPLEMENTED)
#define MULIB_SYMBOLTABLE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_symboltable_free(mulib_symboltable_t *t){if(!t)return;
for(size_t i=0;i<t->count;i++)free(t->items[i].name);
free(t->items);
free(t->scope_starts);
memset(t,0,sizeof(*t));
}
int mulib_symboltable_enter(mulib_symboltable_t *t){if(!t)return 0;
if(t->depth==t->scope_capacity){size_t cap=t->scope_capacity?t->scope_capacity*2:8;
if(cap<t->scope_capacity||cap>SIZE_MAX/sizeof(size_t))return 0;
void *p=realloc(t->scope_starts,cap*sizeof(size_t));
if(!p)return 0;
t->scope_starts=(size_t*)p;
t->scope_capacity=cap;
}t->scope_starts[t->depth++]=t->count;
return 1;
}
int mulib_symboltable_leave(mulib_symboltable_t *t){if(!t||!t->depth)return 0;
size_t n=t->scope_starts[--t->depth];
while(t->count>n)free(t->items[--t->count].name);
return 1;
}
int mulib_symboltable_define(mulib_symboltable_t *t,const char *name,uint64_t value){
 if(!t||!name||!*name)return 0;
size_t start=t->depth?t->scope_starts[t->depth-1]:0;
 for(size_t i=start;i<t->count;i++)if(!strcmp(name,t->items[i].name))return 0;
 size_t n=strlen(name);
char *p=(char*)malloc(n+1);
if(!p)return 0;
memcpy(p,name,n+1);
 if(t->count==t->capacity){size_t cap=t->capacity?t->capacity*2:16;
if(cap<t->capacity||cap>SIZE_MAX/sizeof(*t->items)){free(p);
return 0;
}void *next=realloc(t->items,cap*sizeof(*t->items));
if(!next){free(p);
return 0;
}t->items=(mulib_symboltable_entry_t*)next;
t->capacity=cap;
}
 t->items[t->count++]=(mulib_symboltable_entry_t){p,value,t->depth};
return 1;
}
int mulib_symboltable_lookup(const mulib_symboltable_t *t,const char *name,uint64_t *value){if(!t||!name||!value)return 0;
for(size_t i=t->count;i>0;i--)if(!strcmp(t->items[i-1].name,name)){*value=t->items[i-1].value;
return 1;
}
return 0;
}
#ifdef __cplusplus
}
#endif
#endif
