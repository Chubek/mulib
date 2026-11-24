/* MuLIB compiler tools: Register definition/use counting for compact virtual-register instruction sequences.
 * ISO C11 single-header module. Define MULIB_IMPL_DEFUSE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_DEFUSE_H
#define MULIB_DEFUSE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t destination,source[2];
unsigned uses,effectful;
} mulib_defuse_inst_t;
typedef struct {size_t first_definition,use_count;
} mulib_defuse_info_t;
int mulib_defuse_analyze(const mulib_defuse_inst_t *inst,size_t count,size_t variables,mulib_defuse_info_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DEFUSE_H */
#if defined(MULIB_IMPL_DEFUSE) && !defined(MULIB_DEFUSE_IMPLEMENTED)
#define MULIB_DEFUSE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_defuse_analyze(const mulib_defuse_inst_t *in,size_t count,size_t regs,mulib_defuse_info_t *out){
 if((!in&&count)||(!out&&regs))return 0;
mulib_defuse_info_t *temp=(mulib_defuse_info_t*)calloc(regs?regs:1,sizeof(*temp));
if(!temp)return 0;
 for(size_t r=0;r<regs;r++)temp[r].first_definition=SIZE_MAX;
 for(size_t i=0;i<count;i++){if(in[i].uses>2||(in[i].destination!=SIZE_MAX&&in[i].destination>=regs)){free(temp);
return 0;
}for(unsigned j=0;j<in[i].uses;j++){size_t v=in[i].source[j];
if(v>=regs||temp[v].use_count==SIZE_MAX){free(temp);
return 0;
}temp[v].use_count++;
}
 if(in[i].destination!=SIZE_MAX&&temp[in[i].destination].first_definition==SIZE_MAX)temp[in[i].destination].first_definition=i;
}
 if(regs)memcpy(out,temp,regs*sizeof(*temp));
free(temp);
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
