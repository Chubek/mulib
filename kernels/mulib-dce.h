/* MuLIB compiler tools: Effect-aware backward dead-instruction marking for 64 virtual registers.
 * ISO C11 single-header module. Define MULIB_IMPL_DCE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_DCE_H
#define MULIB_DCE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-defuse.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-defuse.h"
/* marks[i]=1 for retained instruction; effects and live-out are roots. */
int mulib_dce_mark(const mulib_defuse_inst_t *code,size_t count,uint64_t live_out,unsigned char *marks);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DCE_H */
#if defined(MULIB_IMPL_DCE) && !defined(MULIB_DCE_IMPLEMENTED)
#define MULIB_DCE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dce_mark(const mulib_defuse_inst_t *p,size_t n,uint64_t live,unsigned char *marks){if((!p&&n)||(!marks&&n))return 0;
 for(size_t i=0;i<n;i++){
 if(p[i].uses>2||(p[i].destination!=SIZE_MAX&&p[i].destination>=64))return 0;
 for(unsigned j=0;j<p[i].uses;j++)if(p[i].source[j]>=64)return 0;
 }
 for(size_t i=n;i>0;i--){const mulib_defuse_inst_t *ins=p+i-1;
 if(ins->uses>2||(ins->destination!=SIZE_MAX&&ins->destination>=64))return 0;
 for(unsigned j=0;j<ins->uses;j++)if(ins->source[j]>=64)return 0;
 int keep=ins->effectful||(ins->destination!=SIZE_MAX&&(live&(UINT64_C(1)<<ins->destination)));
 marks[i-1]=(unsigned char)keep;
if(!keep)continue;
 if(ins->destination!=SIZE_MAX)live&=~(UINT64_C(1)<<ins->destination);
 for(unsigned j=0;j<ins->uses;j++)live|=UINT64_C(1)<<ins->source[j];
}
 return 1;
}
#ifdef __cplusplus
}
#endif
#endif
