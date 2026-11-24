/* MuLIB compiler tools: Portable physical-register mask filtering and deterministic allocation.
 * ISO C11 single-header module. Define MULIB_IMPL_PHYSREG in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_PHYSREG_H
#define MULIB_PHYSREG_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t available,reserved,caller_saved,callee_saved;
} mulib_physreg_t;
int mulib_physreg_validate(const mulib_physreg_t *r);
int mulib_physreg_pick(const mulib_physreg_t *r,uint64_t occupied,unsigned *index);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PHYSREG_H */
#if defined(MULIB_IMPL_PHYSREG) && !defined(MULIB_PHYSREG_IMPLEMENTED)
#define MULIB_PHYSREG_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_physreg_validate(const mulib_physreg_t *r){return r && !(r->available & r->reserved) && !(r->caller_saved&r->callee_saved) && !((r->caller_saved|r->callee_saved)&~r->available);
}
int mulib_physreg_pick(const mulib_physreg_t *r,uint64_t occupied,unsigned *out){if(!mulib_physreg_validate(r)||!out)return 0;
uint64_t mask=r->available &~occupied;
for(unsigned i=0;i<64;i++)if(mask&(UINT64_C(1)<<i)){*out=i;
return 1;
}
return 0;
}
#ifdef __cplusplus
}
#endif
#endif
