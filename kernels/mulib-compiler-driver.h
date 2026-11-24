/* MuLIB compiler tools: Explicit compiler-stage pipeline with deterministic stop-on-error semantics.
 * ISO C11 single-header module. Define MULIB_IMPL_COMPILER_DRIVER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_COMPILER_DRIVER_H
#define MULIB_COMPILER_DRIVER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_compiler_stage_fn)(void *program,void *context);
typedef struct {const char *name;
mulib_compiler_stage_fn run;
void *context;
} mulib_compiler_stage_t;
typedef struct {mulib_compiler_stage_t *stages;
size_t count,capacity;
} mulib_compiler_driver_t;
int mulib_compiler_driver_add(mulib_compiler_driver_t *d,const char *name,mulib_compiler_stage_fn fn,void *context);
int mulib_compiler_driver_run(const mulib_compiler_driver_t *d,void *program,size_t *failed_index);
void mulib_compiler_driver_free(mulib_compiler_driver_t *d);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPILER_DRIVER_H */
#if defined(MULIB_IMPL_COMPILER_DRIVER) && !defined(MULIB_COMPILER_DRIVER_IMPLEMENTED)
#define MULIB_COMPILER_DRIVER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_compiler_driver_add(mulib_compiler_driver_t *d,const char *name,mulib_compiler_stage_fn fn,void *ctx){if(!d||!name||!fn)return 0;
if(d->count==d->capacity){size_t c=d->capacity?d->capacity*2:8;
if(c<d->capacity||c>SIZE_MAX/sizeof(*d->stages))return 0;
void *p=realloc(d->stages,c*sizeof(*d->stages));
if(!p)return 0;
d->stages=(mulib_compiler_stage_t*)p;
d->capacity=c;
}d->stages[d->count++]=(mulib_compiler_stage_t){name,fn,ctx};
return 1;
}
int mulib_compiler_driver_run(const mulib_compiler_driver_t *d,void *p,size_t *failed){if(!d||!p)return 0;
for(size_t i=0;i<d->count;i++)if(!d->stages[i].run(p,d->stages[i].context)){if(failed)*failed=i;
return 0;
}
if(failed)*failed=d->count;
return 1;
}
void mulib_compiler_driver_free(mulib_compiler_driver_t *d){if(d){free(d->stages);
memset(d,0,sizeof(*d));
}}
#ifdef __cplusplus
}
#endif
#endif
