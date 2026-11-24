/* MuLIB compiler tools: Owned named bytecode modules with checked decoding and module lookup.
 * ISO C11 single-header module. Define MULIB_IMPL_VM_MODULE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_VM_MODULE_H
#define MULIB_VM_MODULE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode-verify.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
typedef struct {char *name;
mulib_bc_instruction_t *code;
size_t count,locals;
} mulib_vm_module_t;
typedef struct {mulib_vm_module_t *modules;
size_t count,capacity;
} mulib_vm_modules_t;
int mulib_vm_module_add(mulib_vm_modules_t *m,const char *name,const mulib_bc_instruction_t *code,size_t count,size_t locals);
/* Code must pass the bytecode verifier; names and instructions are copied. */
const mulib_vm_module_t *mulib_vm_module_find(const mulib_vm_modules_t *m,const char *name);
void mulib_vm_modules_free(mulib_vm_modules_t *m);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VM_MODULE_H */
#if defined(MULIB_IMPL_VM_MODULE) && !defined(MULIB_VM_MODULE_IMPLEMENTED)
#define MULIB_VM_MODULE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
const mulib_vm_module_t *mulib_vm_module_find(const mulib_vm_modules_t *m,const char *name){if(!m||!name)return NULL;
for(size_t i=0;i<m->count;i++)if(!strcmp(m->modules[i].name,name))return m->modules+i;
return NULL;
}
int mulib_vm_module_add(mulib_vm_modules_t *m,const char *name,const mulib_bc_instruction_t *code,size_t n,size_t locals){
 if(!m||!name||!*name||(!code&&n)||mulib_vm_module_find(m,name)||n>SIZE_MAX/sizeof(*code))return 0;
size_t peak;
if(!mulib_bytecode_verify(code,n,locals,&peak,NULL))return 0;
size_t l=strlen(name);
char *s=(char*)malloc(l+1);
mulib_bc_instruction_t *data=(mulib_bc_instruction_t*)malloc((n?n:1)*sizeof(*data));
if(!s||!data){free(s);
free(data);
return 0;
}memcpy(s,name,l+1);
if(n)memcpy(data,code,n*sizeof(*data));
 if(m->count==m->capacity){size_t cap=m->capacity?m->capacity*2:8;
if(cap<m->capacity||cap>SIZE_MAX/sizeof(*m->modules)){free(s);
free(data);
return 0;
}void *p=realloc(m->modules,cap*sizeof(*m->modules));
if(!p){free(s);
free(data);
return 0;
}m->modules=(mulib_vm_module_t*)p;
m->capacity=cap;
}
 m->modules[m->count++]=(mulib_vm_module_t){s,data,n,locals};
return 1;
}
void mulib_vm_modules_free(mulib_vm_modules_t *m){if(!m)return;
for(size_t i=0;i<m->count;i++){free(m->modules[i].name);
free(m->modules[i].code);
}free(m->modules);
memset(m,0,sizeof(*m));
}
#ifdef __cplusplus
}
#endif
#endif
