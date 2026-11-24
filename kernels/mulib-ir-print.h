/* MuLIB compiler tools: Bounded text rendering of the compact verified register IR.
 * ISO C11 single-header module. Define MULIB_IMPL_IR_PRINT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_IR_PRINT_H
#define MULIB_IR_PRINT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-ir-verify.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-ir-verify.h"
/* Result is bytes written excluding NUL; 0 on invalid input or insufficient capacity.
 * On failure, out may contain a NUL-terminated prefix. */
size_t mulib_ir_print(const mulib_ir_instruction_t *code,size_t count,char *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IR_PRINT_H */
#if defined(MULIB_IMPL_IR_PRINT) && !defined(MULIB_IR_PRINT_IMPLEMENTED)
#define MULIB_IR_PRINT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_ir_print(const mulib_ir_instruction_t *code,size_t n,char *out,size_t cap){if((!code&&n)||!out||!cap)return 0;
size_t pos=0;
out[0]=0;
 const char *names[]={"const","param","copy","add","sub","mul","div","eq","return"};
for(size_t i=0;i<n;i++){const mulib_ir_instruction_t *ins=code+i;
if((unsigned)ins->op>MULIB_IR_RETURN)return 0;
 char buffer[160];
int z=snprintf(buffer,sizeof(buffer),"%zu: %s r%zu, r%zu, r%zu, %lld\n",i,names[ins->op],ins->destination,ins->a,ins->b,(long long)ins->immediate);
 if(z<0||(size_t)z>=sizeof(buffer)||(size_t)z>SIZE_MAX-pos)return 0;
 size_t len=(size_t)z;
if(len>=cap-pos)return 0;
memcpy(out+pos,buffer,len);
pos+=len;
out[pos]=0;
}return pos;
}
#ifdef __cplusplus
}
#endif
#endif
