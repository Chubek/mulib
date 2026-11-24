/** @file mulib-docincremental.h
 * @brief Configurable execution policy and accounting for document ir docincremental.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCINCREMENTAL once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docincremental.md for scope and limitations.
 */
#ifndef MULIB_DOCINCREMENTAL_H
#define MULIB_DOCINCREMENTAL_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_docincremental_policy { size_t limit,used; } mulib_docincremental_policy_t;
void mulib_docincremental_init(mulib_docincremental_policy_t *policy, size_t limit);
int mulib_docincremental_charge(mulib_docincremental_policy_t *policy, size_t amount);
void mulib_docincremental_reset(mulib_docincremental_policy_t *policy);
size_t mulib_docincremental_remaining(const mulib_docincremental_policy_t *policy);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCINCREMENTAL_H */
#if defined(MULIB_IMPL_DOCINCREMENTAL) && !defined(MULIB_DOCINCREMENTAL_H_IMPLEMENTED)
#define MULIB_DOCINCREMENTAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_docincremental_init(mulib_docincremental_policy_t *p,size_t limit){if(p){p->limit=limit;p->used=0;}}
int mulib_docincremental_charge(mulib_docincremental_policy_t *p,size_t n){if(!p||p->used>p->limit||n>p->limit-p->used)return 0;p->used+=n;return 1;}
void mulib_docincremental_reset(mulib_docincremental_policy_t *p){if(p)p->used=0;}
size_t mulib_docincremental_remaining(const mulib_docincremental_policy_t *p){return p&&p->used<=p->limit?p->limit-p->used:0;}
#ifdef __cplusplus
}
#endif
#endif
