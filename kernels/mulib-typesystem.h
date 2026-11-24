/* MuLIB compiler tools: Interned structural primitive/pointer/array/function type graph with stable IDs.
 * ISO C11 single-header module. Define MULIB_IMPL_TYPESYSTEM in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_TYPESYSTEM_H
#define MULIB_TYPESYSTEM_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_TYPE_INVALID,MULIB_TYPE_VOID,MULIB_TYPE_BOOL,MULIB_TYPE_I64,MULIB_TYPE_F64,MULIB_TYPE_POINTER,MULIB_TYPE_ARRAY,MULIB_TYPE_FUNCTION} mulib_type_kind_t;
typedef struct {mulib_type_kind_t kind;
size_t element,count,argument;
} mulib_type_t;
typedef struct {mulib_type_t *types;
size_t count,capacity;
} mulib_typesystem_t;
int mulib_typesystem_intern(mulib_typesystem_t *t,mulib_type_t desc,size_t *id);
const mulib_type_t *mulib_typesystem_get(const mulib_typesystem_t *t,size_t id);
void mulib_typesystem_free(mulib_typesystem_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPESYSTEM_H */
#if defined(MULIB_IMPL_TYPESYSTEM) && !defined(MULIB_TYPESYSTEM_IMPLEMENTED)
#define MULIB_TYPESYSTEM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_typesystem_intern(mulib_typesystem_t *t,mulib_type_t d,size_t *id){
 if(!t||!id||d.kind<=MULIB_TYPE_INVALID||d.kind>MULIB_TYPE_FUNCTION)return 0;
 if((d.kind==MULIB_TYPE_POINTER||d.kind==MULIB_TYPE_ARRAY||d.kind==MULIB_TYPE_FUNCTION)&&d.element>=t->count)return 0;
 if(d.kind==MULIB_TYPE_FUNCTION&&d.count&&d.argument>=t->count)return 0;
 if(d.kind<=MULIB_TYPE_F64){d.element=0;
d.count=0;
d.argument=0;
}
 if(d.kind==MULIB_TYPE_POINTER){d.count=0;
d.argument=0;
}
 if(d.kind==MULIB_TYPE_ARRAY)d.argument=0;
 if(d.kind==MULIB_TYPE_FUNCTION&&!d.count)d.argument=0;
 for(size_t i=0;i<t->count;i++){mulib_type_t a=t->types[i];
if(a.kind==d.kind&&a.element==d.element&&a.count==d.count&&a.argument==d.argument){*id=i;
return 1;
}}
 if(t->count==t->capacity){size_t cap=t->capacity?t->capacity*2:8;
if(cap<t->capacity||cap>SIZE_MAX/sizeof(*t->types))return 0;
void *p=realloc(t->types,cap*sizeof(*t->types));
if(!p)return 0;
t->types=(mulib_type_t*)p;
t->capacity=cap;
}
 *id=t->count;
t->types[t->count++]=d;
return 1;
}
const mulib_type_t *mulib_typesystem_get(const mulib_typesystem_t *t,size_t id){return t&&id<t->count?t->types+id:NULL;
}
void mulib_typesystem_free(mulib_typesystem_t *t){if(t){free(t->types);
memset(t,0,sizeof(*t));
}}
#ifdef __cplusplus
}
#endif
#endif
