/* MuLIB compiler tools: Resolve Pratt identifier occurrences into lexical symbol IDs.
 * ISO C11 single-header module. Define MULIB_IMPL_NAME_RESOLUTION in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_NAME_RESOLUTION_H
#define MULIB_NAME_RESOLUTION_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-parser-pratt.h"
#include "mulib-symboltable.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-parser-pratt.h"
#include "mulib-symboltable.h"
/* destination is indexed by node ID, SIZE_MAX for non-identifiers. */
int mulib_name_resolution_run(const mulib_parser_pratt_t *tree,const mulib_symboltable_t *symbols,size_t *out,size_t capacity,size_t *failure_node);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_NAME_RESOLUTION_H */
#if defined(MULIB_IMPL_NAME_RESOLUTION) && !defined(MULIB_NAME_RESOLUTION_IMPLEMENTED)
#define MULIB_NAME_RESOLUTION_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_name_resolution_run(const mulib_parser_pratt_t *t,const mulib_symboltable_t *s,size_t *out,size_t cap,size_t *failure){
 if(!t||(!t->nodes&&t->count)||(!out&&t->count)||cap<t->count||t->count>SIZE_MAX/sizeof(size_t))return 0;
 size_t *tmp=(size_t*)malloc((t->count?t->count:1)*sizeof(size_t));
if(!tmp)return 0;
 for(size_t i=0;i<t->count;i++){tmp[i]=SIZE_MAX;
if(t->nodes[i].opcode==MULIB_PRATT_NAME){uint64_t id;
if(!s||!mulib_symboltable_lookup(s,t->nodes[i].name,&id)||id>=SIZE_MAX){if(failure)*failure=i;
free(tmp);
return 0;
}tmp[i]=(size_t)id;
}}
 if(t->count)memcpy(out,tmp,t->count*sizeof(size_t));
free(tmp);
if(failure)*failure=SIZE_MAX;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
