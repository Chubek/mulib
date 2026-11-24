/** @file mulib-typeincremental.h
 * @brief Configurable execution policy and accounting for infrastructure typeincremental.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPEINCREMENTAL once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typeincremental.md for scope and limitations.
 */
#ifndef MULIB_TYPEINCREMENTAL_H
#define MULIB_TYPEINCREMENTAL_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typeincremental_policy { size_t limit,used; } mulib_typeincremental_policy_t;
void mulib_typeincremental_init(mulib_typeincremental_policy_t *policy, size_t limit);
int mulib_typeincremental_charge(mulib_typeincremental_policy_t *policy, size_t amount);
void mulib_typeincremental_reset(mulib_typeincremental_policy_t *policy);
size_t mulib_typeincremental_remaining(const mulib_typeincremental_policy_t *policy);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPEINCREMENTAL_H */
#if defined(MULIB_IMPL_TYPEINCREMENTAL) && !defined(MULIB_TYPEINCREMENTAL_H_IMPLEMENTED)
#define MULIB_TYPEINCREMENTAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_typeincremental_init(mulib_typeincremental_policy_t *p,size_t limit){if(p){p->limit=limit;p->used=0;}}
int mulib_typeincremental_charge(mulib_typeincremental_policy_t *p,size_t n){if(!p||p->used>p->limit||n>p->limit-p->used)return 0;p->used+=n;return 1;}
void mulib_typeincremental_reset(mulib_typeincremental_policy_t *p){if(p)p->used=0;}
size_t mulib_typeincremental_remaining(const mulib_typeincremental_policy_t *p){return p&&p->used<=p->limit?p->limit-p->used:0;}
#ifdef __cplusplus
}
#endif
#endif
