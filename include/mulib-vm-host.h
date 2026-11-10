/* MuLIB compiler tools: Bounded host-call registry with explicit arity and copied function descriptors.
 * ISO C11 single-header module. Define MULIB_IMPL_VM_HOST in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_VM_HOST_H
#define MULIB_VM_HOST_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_vm_host_fn)(void *user,const int64_t *args,size_t argc,int64_t *result);
typedef struct {mulib_vm_host_fn fn;
void *user;
size_t arity;
} mulib_vm_host_entry_t;
typedef struct {mulib_vm_host_entry_t *entries;
size_t count,capacity;
} mulib_vm_host_t;
int mulib_vm_host_register(mulib_vm_host_t *h,mulib_vm_host_fn fn,void *user,size_t arity,size_t *id);
int mulib_vm_host_call(const mulib_vm_host_t *h,size_t id,const int64_t *args,size_t argc,int64_t *out);
void mulib_vm_host_free(mulib_vm_host_t *h);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VM_HOST_H */
#if defined(MULIB_IMPL_VM_HOST) && !defined(MULIB_VM_HOST_IMPLEMENTED)
#define MULIB_VM_HOST_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_vm_host_register(mulib_vm_host_t *h,mulib_vm_host_fn fn,void *user,size_t arity,size_t *id){if(!h||!fn||!id||arity>1024)return 0;
if(h->count==h->capacity){size_t n=h->capacity?h->capacity*2:8;
if(n<h->capacity||n>SIZE_MAX/sizeof(*h->entries))return 0;
void *p=realloc(h->entries,n*sizeof(*h->entries));
if(!p)return 0;
h->entries=(mulib_vm_host_entry_t*)p;
h->capacity=n;
}*id=h->count;
h->entries[h->count++]=(mulib_vm_host_entry_t){fn,user,arity};
return 1;
}
int mulib_vm_host_call(const mulib_vm_host_t *h,size_t id,const int64_t *args,size_t argc,int64_t *out){if(!h||id>=h->count||!out||argc!=h->entries[id].arity||(!args&&argc))return 0;
int64_t v=0;
if(!h->entries[id].fn(h->entries[id].user,args,argc,&v))return 0;
*out=v;
return 1;
}
void mulib_vm_host_free(mulib_vm_host_t *h){if(h){free(h->entries);
memset(h,0,sizeof(*h));
}}
#ifdef __cplusplus
}
#endif
#endif
