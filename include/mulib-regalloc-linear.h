/* MuLIB compiler tools: Linear-scan allocation with expiration, longest-end eviction and stable spill slots.
 * ISO C11 single-header module. Define MULIB_IMPL_REGALLOC_LINEAR in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_REGALLOC_LINEAR_H
#define MULIB_REGALLOC_LINEAR_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-physreg.h"
#include "mulib-liveintervals.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-physreg.h"
#include "mulib-liveintervals.h"
typedef struct {int reg;
size_t spill;
} mulib_regalloc_location_t;
/* locations indexed by input interval, reg=-1 means spilled. */
int mulib_regalloc_linear(const mulib_liveinterval_t *ranges,size_t n,uint64_t registers,mulib_regalloc_location_t *locations,size_t *spill_count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_REGALLOC_LINEAR_H */
#if defined(MULIB_IMPL_REGALLOC_LINEAR) && !defined(MULIB_REGALLOC_LINEAR_IMPLEMENTED)
#define MULIB_REGALLOC_LINEAR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_regalloc_linear(const mulib_liveinterval_t *ranges,size_t n,uint64_t regs,mulib_regalloc_location_t *out,size_t *spill_count){
 if((!ranges&&n)||(!out&&n)||!spill_count||n>65536)return 0;
 size_t *order=(size_t*)malloc((n?n:1)*sizeof(size_t));
mulib_regalloc_location_t *loc=(mulib_regalloc_location_t*)malloc((n?n:1)*sizeof(*loc));
 if(!order||!loc){free(order);
free(loc);
return 0;
}
 for(size_t i=0;i<n;i++){if(!mulib_liveinterval_valid(ranges+i)){free(order);
free(loc);
return 0;
}order[i]=i;
loc[i].reg=-1;
loc[i].spill=SIZE_MAX;
}
 /* Stable sorted starts; worst O(n^2), bounded at 65536. */
 for(size_t i=1;i<n;i++){size_t v=order[i],j=i;
while(j>0&&(ranges[order[j-1]].start>ranges[v].start||(ranges[order[j-1]].start==ranges[v].start&&order[j-1]>v))){order[j]=order[j-1];
j--;
}order[j]=v;
}
 size_t spills=0;
for(size_t k=0;k<n;k++){size_t id=order[k];
uint64_t occupied=0;
 for(size_t j=0;j<k;j++){size_t other=order[j];
if(loc[other].reg>=0&&ranges[other].end>=ranges[id].start)occupied|=UINT64_C(1)<<(unsigned)loc[other].reg;
}
 uint64_t freebits=regs&~occupied;
int chosen=-1;
for(unsigned j=0;j<64;j++)if(freebits&(UINT64_C(1)<<j)){chosen=(int)j;
break;
}
 if(chosen>=0){loc[id].reg=chosen;
continue;
}
 size_t victim=SIZE_MAX;
 for(size_t j=0;j<k;j++){size_t other=order[j];
if(loc[other].reg>=0&&ranges[other].end>=ranges[id].start&&(victim==SIZE_MAX||ranges[other].end>ranges[victim].end))victim=other;
}
 if(victim!=SIZE_MAX&&ranges[victim].end>ranges[id].end){loc[id].reg=loc[victim].reg;
loc[victim].reg=-1;
loc[victim].spill=spills++;
}
 else loc[id].spill=spills++;
 }
 if(n)memcpy(out,loc,n*sizeof(*out));
*spill_count=spills;
free(order);
free(loc);
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
