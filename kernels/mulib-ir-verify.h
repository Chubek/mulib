/* MuLIB compiler tools: Verification of a compact register IR with dominance-free linear definition ordering.
 * ISO C11 single-header module. Define MULIB_IMPL_IR_VERIFY in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_IR_VERIFY_H
#define MULIB_IR_VERIFY_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_IR_LITERAL,MULIB_IR_PARAM,MULIB_IR_COPY,MULIB_IR_ADD,MULIB_IR_SUB,MULIB_IR_MUL,MULIB_IR_DIV,MULIB_IR_EQ,MULIB_IR_RETURN} mulib_ir_opcode_t;
typedef struct {mulib_ir_opcode_t op;
size_t destination,a,b;
int64_t immediate;
} mulib_ir_instruction_t;
/* Values are defined by literal/param/copy/arithmetic in program order. */
int mulib_ir_verify(const mulib_ir_instruction_t *code,size_t count,size_t register_count,size_t *bad_index);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IR_VERIFY_H */
#if defined(MULIB_IMPL_IR_VERIFY) && !defined(MULIB_IR_VERIFY_IMPLEMENTED)
#define MULIB_IR_VERIFY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ir_verify(const mulib_ir_instruction_t *p,size_t n,size_t regs,size_t *bad){
 if((!p&&n)||regs>1048576)return 0;
unsigned char *defined=(unsigned char*)calloc(regs?regs:1,1);
if(!defined)return 0;
int saw_return=0;
 for(size_t i=0;i<n;i++){const mulib_ir_instruction_t *ins=p+i;
int arity=0,defines=1;
 switch(ins->op){case MULIB_IR_LITERAL:case MULIB_IR_PARAM:break;
case MULIB_IR_COPY:arity=1;
break;
 case MULIB_IR_ADD:case MULIB_IR_SUB:case MULIB_IR_MUL:case MULIB_IR_DIV:case MULIB_IR_EQ:arity=2;
break;
 case MULIB_IR_RETURN:arity=1;
defines=0;
break;
default:goto bad;
}
 if((arity>0&&(ins->a>=regs||!defined[ins->a]))||(arity>1&&(ins->b>=regs||!defined[ins->b]))||(defines&&(ins->destination>=regs||defined[ins->destination]))||saw_return)goto bad;
 if(defines)defined[ins->destination]=1;
else saw_return=1;
continue;
 bad:if(bad)*bad=i;
free(defined);
return 0;
}
 free(defined);
if(!saw_return){if(bad)*bad=n;
return 0;
}if(bad)*bad=SIZE_MAX;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
