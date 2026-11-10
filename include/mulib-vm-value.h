/* MuLIB compiler tools: Owned tagged VM scalars, binary strings, and deep-copy conversion helpers.
 * ISO C11 single-header module. Define MULIB_IMPL_VM_VALUE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_VM_VALUE_H
#define MULIB_VM_VALUE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_VM_NIL,MULIB_VM_BOOL,MULIB_VM_I64,MULIB_VM_F64,MULIB_VM_BYTES} mulib_vm_value_kind_t;
typedef struct {mulib_vm_value_kind_t kind;
union {int boolean;
int64_t integer;
double real;
struct {unsigned char *bytes;
size_t length;
} str;
} value;
} mulib_vm_value_t;
void mulib_vm_value_free(mulib_vm_value_t *v);
int mulib_vm_value_copy(mulib_vm_value_t *dst,const mulib_vm_value_t *src);
int mulib_vm_value_bytes(mulib_vm_value_t *v,const void *data,size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VM_VALUE_H */
#if defined(MULIB_IMPL_VM_VALUE) && !defined(MULIB_VM_VALUE_IMPLEMENTED)
#define MULIB_VM_VALUE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_vm_value_free(mulib_vm_value_t *v){if(!v)return;
if(v->kind==MULIB_VM_BYTES)free(v->value.str.bytes);
memset(v,0,sizeof(*v));
}
int mulib_vm_value_bytes(mulib_vm_value_t *v,const void *data,size_t n){if(!v||(!data&&n))return 0;
unsigned char *copy=NULL;
if(n){copy=(unsigned char*)malloc(n);
if(!copy)return 0;
memcpy(copy,data,n);
}mulib_vm_value_free(v);
v->kind=MULIB_VM_BYTES;
v->value.str.bytes=copy;
v->value.str.length=n;
return 1;
}
int mulib_vm_value_copy(mulib_vm_value_t *dst,const mulib_vm_value_t *src){if(!dst||!src||(unsigned)src->kind>MULIB_VM_BYTES)return 0;
if(dst==src)return 1;
if(src->kind==MULIB_VM_BYTES)return mulib_vm_value_bytes(dst,src->value.str.bytes,src->value.str.length);
if(src->kind>MULIB_VM_BYTES)return 0;
mulib_vm_value_free(dst);
*dst=*src;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
