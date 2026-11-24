/* MuLIB compiler tools: Portable framed little-endian bytecode codec with FNV checksum and exact decoding.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_SERIALIZE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_SERIALIZE_H
#define MULIB_BYTECODE_SERIALIZE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
/* Format: MBC1, u32 instruction count, N*(u8 opcode,u64 operand), u32 FNV32. */
int mulib_bytecode_encode(const mulib_bc_instruction_t *code,size_t count,unsigned char **bytes,size_t *length);
int mulib_bytecode_decode(const void *bytes,size_t length,mulib_bc_instruction_t **code,size_t *count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_SERIALIZE_H */
#if defined(MULIB_IMPL_BYTECODE_SERIALIZE) && !defined(MULIB_BYTECODE_SERIALIZE_IMPLEMENTED)
#define MULIB_BYTECODE_SERIALIZE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static uint32_t mulib_bytecode_hash_(const unsigned char *p,size_t n){uint32_t h=UINT32_C(2166136261);
for(size_t i=0;i<n;i++){h^=p[i];
h*=UINT32_C(16777619);
}return h;
}
static void mulib_bytecode_u32_(unsigned char *p,uint32_t v){for(unsigned j=0;j<4;j++)p[j]=(unsigned char)(v>>(8*j));
}
static uint32_t mulib_bytecode_rd32_(const unsigned char *p){uint32_t v=0;
for(unsigned j=0;j<4;j++)v|=(uint32_t)p[j]<<(8*j);
return v;
}
int mulib_bytecode_encode(const mulib_bc_instruction_t *code,size_t n,unsigned char **bytes,size_t *length){if((!code&&n)||!bytes||!length||n>1048576||*bytes)return 0;
size_t z=12+n*9;
unsigned char *p=(unsigned char*)malloc(z);
if(!p)return 0;
memcpy(p,"MBC1",4);
mulib_bytecode_u32_(p+4,(uint32_t)n);
 for(size_t i=0;i<n;i++){if((unsigned)code[i].op>MULIB_BC_HALT){free(p);
return 0;
}p[8+i*9]=(unsigned char)code[i].op;
uint64_t v=(uint64_t)code[i].operand;
for(unsigned j=0;j<8;j++)p[8+i*9+1+j]=(unsigned char)(v>>(8*j));
}
 mulib_bytecode_u32_(p+8+n*9,mulib_bytecode_hash_(p,8+n*9));
*bytes=p;
*length=z;
return 1;
}
int mulib_bytecode_decode(const void *data,size_t n,mulib_bc_instruction_t **out,size_t *count){
 if(!data||n<12||!out||!count||*out)return 0;
const unsigned char *p=(const unsigned char*)data;
 if(memcmp(p,"MBC1",4))return 0;
uint32_t entries=mulib_bytecode_rd32_(p+4);
if(entries>1048576||n!=12+(size_t)entries*9||mulib_bytecode_rd32_(p+8+(size_t)entries*9)!=mulib_bytecode_hash_(p,n-4))return 0;
 mulib_bc_instruction_t *arr=(mulib_bc_instruction_t*)malloc((entries?entries:1)*sizeof(*arr));
if(!arr)return 0;
 for(size_t i=0;i<entries;i++){unsigned op=p[8+i*9];
if(op>MULIB_BC_HALT){free(arr);
return 0;
}uint64_t v=0;
for(unsigned j=0;j<8;j++)v|=(uint64_t)p[8+i*9+1+j]<<(8*j);
int64_t signed_value;
 if(v<=(uint64_t)INT64_MAX)signed_value=(int64_t)v;
 else {uint64_t mag=(~v)+1;
 signed_value=mag==(uint64_t)INT64_MAX+1?INT64_MIN:-(int64_t)mag;
} arr[i]=(mulib_bc_instruction_t){(mulib_bc_opcode_t)op,signed_value};
}
 *out=arr;
*count=(size_t)entries;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
