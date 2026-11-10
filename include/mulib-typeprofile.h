/** @file mulib-typeprofile.h
 * @brief Configurable execution policy and accounting for infrastructure typeprofile.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPEPROFILE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typeprofile.md for scope and limitations.
 */
#ifndef MULIB_TYPEPROFILE_H
#define MULIB_TYPEPROFILE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typeprofile_policy { size_t limit,used; } mulib_typeprofile_policy_t;
void mulib_typeprofile_init(mulib_typeprofile_policy_t *policy, size_t limit);
int mulib_typeprofile_charge(mulib_typeprofile_policy_t *policy, size_t amount);
void mulib_typeprofile_reset(mulib_typeprofile_policy_t *policy);
size_t mulib_typeprofile_remaining(const mulib_typeprofile_policy_t *policy);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPEPROFILE_H */
#if defined(MULIB_IMPL_TYPEPROFILE) && !defined(MULIB_TYPEPROFILE_H_IMPLEMENTED)
#define MULIB_TYPEPROFILE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_typeprofile_init(mulib_typeprofile_policy_t *p,size_t limit){if(p){p->limit=limit;p->used=0;}}
int mulib_typeprofile_charge(mulib_typeprofile_policy_t *p,size_t n){if(!p||p->used>p->limit||n>p->limit-p->used)return 0;p->used+=n;return 1;}
void mulib_typeprofile_reset(mulib_typeprofile_policy_t *p){if(p)p->used=0;}
size_t mulib_typeprofile_remaining(const mulib_typeprofile_policy_t *p){return p&&p->used<=p->limit?p->limit-p->used:0;}
#ifdef __cplusplus
}
#endif
#endif
