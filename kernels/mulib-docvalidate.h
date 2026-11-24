/** @file mulib-docvalidate.h
 * @brief Legacy budget accounting and opt-in complete semantic DocIR validation.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCVALIDATE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docvalidate.md for scope and limitations.
 */
#ifndef MULIB_DOCVALIDATE_H
#define MULIB_DOCVALIDATE_H
#include "mulib-typecore.h"
#include "mulib-docir.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_docvalidate_policy { size_t limit,used; } mulib_docvalidate_policy_t;
void mulib_docvalidate_init(mulib_docvalidate_policy_t *policy, size_t limit);
int mulib_docvalidate_charge(mulib_docvalidate_policy_t *policy, size_t amount);
void mulib_docvalidate_reset(mulib_docvalidate_policy_t *policy);
size_t mulib_docvalidate_remaining(const mulib_docvalidate_policy_t *policy);
/* Requires MULIB_ENABLE_DOCIR and the docir implementation; checks a complete tree. */
int mulib_docvalidate_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,mulib_docir_report_t *report);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCVALIDATE_H */
#if defined(MULIB_IMPL_DOCVALIDATE) && !defined(MULIB_DOCVALIDATE_H_IMPLEMENTED)
#define MULIB_DOCVALIDATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_docvalidate_init(mulib_docvalidate_policy_t *p,size_t limit){if(p){p->limit=limit;p->used=0;}}
int mulib_docvalidate_charge(mulib_docvalidate_policy_t *p,size_t n){if(!p||p->used>p->limit||n>p->limit-p->used)return 0;p->used+=n;return 1;}
void mulib_docvalidate_reset(mulib_docvalidate_policy_t *p){if(p)p->used=0;}
size_t mulib_docvalidate_remaining(const mulib_docvalidate_policy_t *p){return p&&p->used<=p->limit?p->limit-p->used:0;}
int mulib_docvalidate_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,mulib_docir_report_t *report) {
#ifdef MULIB_ENABLE_DOCIR
    mulib_docir_snapshot_t snapshot={0};
    if(!mulib_docir_snapshot_make(root,limits,&snapshot,report))return 0;
    int ok=mulib_docir_snapshot_validate(&snapshot,mulib_docir_CHECK_DOCUMENT|mulib_docir_CHECK_REFERENCES,report);
    mulib_docir_snapshot_destroy(&snapshot);return ok;
#else
    (void)root;(void)limits;if(report)*report=(mulib_docir_report_t){mulib_docir_E_UNSUPPORTED,0};return 0;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
