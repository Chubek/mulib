/** @file mulib-typefuzz.h
 * @brief Configurable execution policy and accounting for infrastructure typefuzz.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPEFUZZ once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typefuzz.md for scope and limitations.
 */
#ifndef MULIB_TYPEFUZZ_H
#define MULIB_TYPEFUZZ_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typefuzz_policy { size_t limit,used; } mulib_typefuzz_policy_t;
void mulib_typefuzz_init(mulib_typefuzz_policy_t *policy, size_t limit);
int mulib_typefuzz_charge(mulib_typefuzz_policy_t *policy, size_t amount);
void mulib_typefuzz_reset(mulib_typefuzz_policy_t *policy);
size_t mulib_typefuzz_remaining(const mulib_typefuzz_policy_t *policy);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPEFUZZ_H */
#if defined(MULIB_IMPL_TYPEFUZZ) && !defined(MULIB_TYPEFUZZ_H_IMPLEMENTED)
#define MULIB_TYPEFUZZ_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_typefuzz_init(mulib_typefuzz_policy_t *p,size_t limit){if(p){p->limit=limit;p->used=0;}}
int mulib_typefuzz_charge(mulib_typefuzz_policy_t *p,size_t n){if(!p||p->used>p->limit||n>p->limit-p->used)return 0;p->used+=n;return 1;}
void mulib_typefuzz_reset(mulib_typefuzz_policy_t *p){if(p)p->used=0;}
size_t mulib_typefuzz_remaining(const mulib_typefuzz_policy_t *p){return p&&p->used<=p->limit?p->limit-p->used:0;}
#ifdef __cplusplus
}
#endif
#endif
