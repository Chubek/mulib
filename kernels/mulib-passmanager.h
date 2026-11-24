/* MuLIB compiler tools: Bounded ordered compiler pass pipeline with analysis invalidation bits.
 * ISO C11 single-header module. Define MULIB_IMPL_PASSMANAGER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_PASSMANAGER_H
#define MULIB_PASSMANAGER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_pass_fn)(void *program,void *context);
typedef struct {mulib_pass_fn callback;
void *context;
uint64_t preserves;
} mulib_pass_t;
typedef struct {mulib_pass_t *passes;
size_t count,capacity;
uint64_t valid_analyses;
} mulib_passmanager_t;
int mulib_passmanager_add(mulib_passmanager_t *p,mulib_pass_fn fn,void *context,uint64_t preserves);
/* Stops on first failing pass; program effects of earlier passes are retained. */
int mulib_passmanager_run(mulib_passmanager_t *p,void *program,size_t *failed_index);
void mulib_passmanager_free(mulib_passmanager_t *p);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PASSMANAGER_H */
#if defined(MULIB_IMPL_PASSMANAGER) && !defined(MULIB_PASSMANAGER_IMPLEMENTED)
#define MULIB_PASSMANAGER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_passmanager_add(mulib_passmanager_t *p,mulib_pass_fn fn,void *ctx,uint64_t mask){if(!p||!fn)return 0;
if(p->count==p->capacity){size_t z=p->capacity?p->capacity*2:8;
if(z<p->capacity||z>SIZE_MAX/sizeof(*p->passes))return 0;
void *v=realloc(p->passes,z*sizeof(*p->passes));
if(!v)return 0;
p->passes=(mulib_pass_t*)v;
p->capacity=z;
}p->passes[p->count++]=(mulib_pass_t){fn,ctx,mask};
return 1;
}
int mulib_passmanager_run(mulib_passmanager_t *p,void *program,size_t *failed){if(!p||!program)return 0;
for(size_t i=0;i<p->count;i++){int ok=p->passes[i].callback(program,p->passes[i].context);
p->valid_analyses&=p->passes[i].preserves;
if(!ok){if(failed)*failed=i;
return 0;
}
}if(failed)*failed=p->count;
return 1;
}
void mulib_passmanager_free(mulib_passmanager_t *p){if(p){free(p->passes);
memset(p,0,sizeof(*p));
}}
#ifdef __cplusplus
}
#endif
#endif
