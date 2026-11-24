/* MuLIB compiler tools: Textual disassembly of the stable MuLIB stack bytecode instruction subset.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_DISASM in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_DISASM_H
#define MULIB_BYTECODE_DISASM_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
size_t mulib_bytecode_disassemble(const mulib_bc_instruction_t *code,size_t count,char *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_DISASM_H */
#if defined(MULIB_IMPL_BYTECODE_DISASM) && !defined(MULIB_BYTECODE_DISASM_IMPLEMENTED)
#define MULIB_BYTECODE_DISASM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_bytecode_disassemble(const mulib_bc_instruction_t *p,size_t n,char *out,size_t cap){
 if((!p&&n)||!out||!cap)return 0;
static const char *names[]={"PUSH","LOAD","STORE","ADD","SUB","MUL","DIV","EQ","JMP","JZ","HALT"};
size_t pos=0;
out[0]=0;
 for(size_t i=0;i<n;i++){if((unsigned)p[i].op>MULIB_BC_HALT)return 0;
char b[120];
int len=snprintf(b,sizeof(b),"%zu: %s %lld\n",i,names[p[i].op],(long long)p[i].operand);
if(len<0||(size_t)len>=sizeof(b)||(size_t)len>=cap-pos)return 0;
memcpy(out+pos,b,(size_t)len);
pos+=(size_t)len;
out[pos]=0;
}return pos;
}
#ifdef __cplusplus
}
#endif
#endif
