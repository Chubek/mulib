/* MuLIB compiler tools: Fixed-point branch displacement relaxation for short and near x86 branches.
 * ISO C11 single-header module. Define MULIB_IMPL_BRANCH_LAYOUT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BRANCH_LAYOUT_H
#define MULIB_BRANCH_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t target;
size_t short_size,long_size;
int conditional;
} mulib_branch_t;
/* Every instruction has a fixed nonbranch length or an optional branch descriptor. */
int mulib_branch_layout(size_t count,const size_t *fixed,const mulib_branch_t *branches,size_t *offsets,unsigned char *long_form,size_t *total);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BRANCH_LAYOUT_H */
#if defined(MULIB_IMPL_BRANCH_LAYOUT) && !defined(MULIB_BRANCH_LAYOUT_IMPLEMENTED)
#define MULIB_BRANCH_LAYOUT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_branch_layout(size_t n,const size_t *fixed,const mulib_branch_t *br,size_t *out,unsigned char *wide,size_t *total){
 if(!n||!fixed||!br||!out||!wide||!total||n>65536)return 0;
 unsigned char *tmp=(unsigned char*)calloc(n,1);
size_t *pos=(size_t*)malloc(n*sizeof(size_t));
if(!tmp||!pos){free(tmp);
free(pos);
return 0;
}
 for(size_t i=0;i<n;i++)if(br[i].target!=SIZE_MAX&&(br[i].target>=n||!br[i].short_size||br[i].long_size<br[i].short_size)){free(tmp);
free(pos);
return 0;
}
 size_t size=0;
int changed;
size_t steps=0;
do{size=0;
changed=0;
for(size_t i=0;i<n;i++){pos[i]=size;
size_t z=br[i].target==SIZE_MAX?fixed[i]:(tmp[i]?br[i].long_size:br[i].short_size);
if(size>SIZE_MAX-z){free(tmp);
free(pos);
return 0;
}size+=z;
}
 for(size_t i=0;i<n;i++)if(br[i].target!=SIZE_MAX&&!tmp[i]){size_t finish=pos[i]+br[i].short_size,target=pos[br[i].target];
if((target>=finish&&target-finish>127)||(target<finish&&finish-target>128)){tmp[i]=1;
changed=1;
}}
 if(++steps>n+1){free(tmp);
free(pos);
return 0;
}}while(changed);
 size=0;
for(size_t i=0;i<n;i++){pos[i]=size;
size_t z=br[i].target==SIZE_MAX?fixed[i]:(tmp[i]?br[i].long_size:br[i].short_size);
size+=z;
}
 for(size_t i=0;i<n;i++)if(br[i].target!=SIZE_MAX&&tmp[i]){
 size_t finish=pos[i]+br[i].long_size,target=pos[br[i].target];
 if((target>=finish&&target-finish>(uint64_t)INT32_MAX)||
 (target<finish&&finish-target>(uint64_t)INT32_MAX+1)){free(tmp);free(pos);return 0;}
 }
 memcpy(out,pos,n*sizeof(size_t));
memcpy(wide,tmp,n);
*total=size;
free(tmp);
free(pos);
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
