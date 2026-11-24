/** @file mulib-cdd-execute.h
 * @brief Validated plan dispatch and compensating reverse-order rollback.
 * ISO C11; define MULIB_IMPL_CDD_EXECUTE in exactly one translation unit.
 * See manual/mulib-cdd-execute.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_EXECUTE_H
#define MULIB_CDD_EXECUTE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-plan.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_cdd_step_fn)(const mulib_cdd_entry_t *,void *context);
typedef struct { mulib_cdd_step_fn apply,undo; void *context; } mulib_cdd_executor_t;
typedef struct { size_t completed,failed_step,undo_failed,undo_attempted; int callback_code; } mulib_cdd_exec_report_t;
/* On apply failure, undo completed steps in reverse; failed step must self-clean.
 * undo may be NULL (no rollback). Callback success is 0, nonzero is failure.
 * The caller supplies approved callbacks; implementation labels are only data.
 * Keep inputs and borrowed payloads live and immutable throughout the call. */
mulib_cdd_status_t mulib_cdd_execute(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,const mulib_cdd_plan_t *plan,const mulib_cdd_executor_t *executor,mulib_cdd_exec_report_t *report);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_EXECUTE_H */
#if defined(MULIB_IMPL_CDD_EXECUTE) && !defined(MULIB_CDD_EXECUTE_IMPLEMENTED)
#define MULIB_CDD_EXECUTE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_cdd_status_t mulib_cdd_execute(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,const mulib_cdd_plan_t *plan,const mulib_cdd_executor_t *x,mulib_cdd_exec_report_t *r) {
    if (!r) return MULIB_CDD_INVALID;
    memset(r,0,sizeof(*r)); r->failed_step=MULIB_CDD_NONE;
    if (!x || !x->apply) return MULIB_CDD_INVALID;
    mulib_cdd_status_t s=mulib_cdd_plan_validate(c,p,plan,&r->failed_step); if (s) return s;
    r->failed_step=MULIB_CDD_NONE;
    for (size_t i=0;i<plan->count;i++) {
        int rc=x->apply(c->entries+plan->order[i],x->context);
        if (rc) {
            r->failed_step=i; r->callback_code=rc;
            if (x->undo) for (size_t j=i;j>0;j--) {
                r->undo_attempted++;
                if (x->undo(c->entries+plan->order[j-1],x->context)) r->undo_failed++;
            }
            return MULIB_CDD_CALLBACK;
        }
        r->completed++;
    }
    return MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
