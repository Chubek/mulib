/* MuLIB compiler tools: Owned append and patch builder for MuLIB stack bytecode.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_BUILDER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_BUILDER_H
#define MULIB_BYTECODE_BUILDER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
typedef struct {mulib_bc_instruction_t *code;
size_t count,capacity;
} mulib_bytecode_builder_t;
int mulib_bytecode_builder_add(mulib_bytecode_builder_t *b,mulib_bc_opcode_t opcode,int64_t operand,size_t *index);
int mulib_bytecode_builder_patch(mulib_bytecode_builder_t *b,size_t index,int64_t operand);
void mulib_bytecode_builder_free(mulib_bytecode_builder_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_BUILDER_H */
#if defined(MULIB_IMPL_BYTECODE_BUILDER) && !defined(MULIB_BYTECODE_BUILDER_IMPLEMENTED)
#define MULIB_BYTECODE_BUILDER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bytecode_builder_add(mulib_bytecode_builder_t *b,mulib_bc_opcode_t op,int64_t val,size_t *index){if(!b||(unsigned)op>MULIB_BC_HALT||b->count>=1048576)return 0;
if(b->count==b->capacity){size_t cap=b->capacity?b->capacity*2:16;
if(cap>1048576)cap=1048576;
void *p=realloc(b->code,cap*sizeof(*b->code));
if(!p)return 0;
b->code=(mulib_bc_instruction_t*)p;
b->capacity=cap;
}if(index)*index=b->count;
b->code[b->count++]=(mulib_bc_instruction_t){op,val};
return 1;
}
int mulib_bytecode_builder_patch(mulib_bytecode_builder_t *b,size_t index,int64_t val){if(!b||index>=b->count)return 0;
b->code[index].operand=val;
return 1;
}
void mulib_bytecode_builder_free(mulib_bytecode_builder_t *b){if(b){free(b->code);
memset(b,0,sizeof(*b));
}}
#ifdef __cplusplus
}
#endif
#endif
