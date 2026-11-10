/* MuLIB compiler tools: Owned name-to-source module table with exact module name lookup.
 * ISO C11 single-header module. Define MULIB_IMPL_MODULE_RESOLVER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_MODULE_RESOLVER_H
#define MULIB_MODULE_RESOLVER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {char *name,*path;
} mulib_module_entry_t;
typedef struct {mulib_module_entry_t *entries;
size_t count,capacity;
} mulib_module_resolver_t;
int mulib_module_resolver_add(mulib_module_resolver_t *r,const char *name,const char *path);
const char *mulib_module_resolver_get(const mulib_module_resolver_t *r,const char *name);
void mulib_module_resolver_free(mulib_module_resolver_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MODULE_RESOLVER_H */
#if defined(MULIB_IMPL_MODULE_RESOLVER) && !defined(MULIB_MODULE_RESOLVER_IMPLEMENTED)
#define MULIB_MODULE_RESOLVER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
const char *mulib_module_resolver_get(const mulib_module_resolver_t *r,const char *n){if(!r||!n)return NULL;
for(size_t i=0;i<r->count;i++)if(!strcmp(r->entries[i].name,n))return r->entries[i].path;
return NULL;
}
int mulib_module_resolver_add(mulib_module_resolver_t *r,const char *name,const char *path){
 if(!r||!name||!*name||!path||mulib_module_resolver_get(r,name))return 0;
 size_t n=strlen(name),p=strlen(path);
if(n==SIZE_MAX||p==SIZE_MAX)return 0;
char *a=(char*)malloc(n+1),*b=(char*)malloc(p+1);
if(!a||!b){free(a);
free(b);
return 0;
}memcpy(a,name,n+1);
memcpy(b,path,p+1);
 if(r->count==r->capacity){size_t cap=r->capacity?r->capacity*2:8;
if(cap<r->capacity||cap>SIZE_MAX/sizeof(*r->entries)){free(a);
free(b);
return 0;
}void *x=realloc(r->entries,cap*sizeof(*r->entries));
if(!x){free(a);
free(b);
return 0;
}r->entries=(mulib_module_entry_t*)x;
r->capacity=cap;
}
 r->entries[r->count++]=(mulib_module_entry_t){a,b};
return 1;
}
void mulib_module_resolver_free(mulib_module_resolver_t *r){if(!r)return;
for(size_t i=0;i<r->count;i++){free(r->entries[i].name);
free(r->entries[i].path);
}free(r->entries);
memset(r,0,sizeof(*r));
}
#ifdef __cplusplus
}
#endif
#endif
