/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_EXECUTE_H
#define MULIB_NB_EXECUTE_H
#include "mulib-nb-runtime.h"
#include "mulib-nb-core.h"
typedef enum {MULIB_NB_RUN_QUEUED=1,MULIB_NB_RUN_RUNNING,MULIB_NB_RUN_DONE,MULIB_NB_RUN_FAILED,MULIB_NB_RUN_CANCELLED} mulib_nb_run_state;
typedef struct {uint64_t request,cell,revision,next_sequence;mulib_nb_run_state state;} mulib_nb_run;
static inline int mulib_nb_execute_begin(mulib_nb_run *r,const mulib_nb_cell *c,uint64_t request,int explicitly_authorized){if(!r||!c||!request||c->kind!=MULIB_NB_CODE||!explicitly_authorized)return 0;*r=(mulib_nb_run){request,c->id,c->revision,0,MULIB_NB_RUN_QUEUED};return 1;}
/* Runtime callback need only accept submission; messages are delivered separately. */
static inline int mulib_nb_execute_start(mulib_nb_run *r,const mulib_nb_cell *c,const mulib_nb_runtimes *rt,mulib_nb_emit_fn emit,void *user){if(!r||!c||r->state!=MULIB_NB_RUN_QUEUED||r->cell!=c->id||r->revision!=c->revision||!emit)return 0;const mulib_nb_runtime_ops *op=mulib_nb_runtime_find(rt,c->language);if(!op)return 0;r->state=MULIB_NB_RUN_RUNNING;if(!op->execute(op->user,r->request,r->cell,c->source,emit,user)){r->state=MULIB_NB_RUN_FAILED;return 0;}return 1;}
static inline int mulib_nb_execute_message(mulib_nb_run *r,const mulib_nb_message *m){if(!r||!m||r->state!=MULIB_NB_RUN_RUNNING||r->request!=m->request||r->cell!=m->cell||m->sequence!=r->next_sequence||r->next_sequence==UINT64_MAX||m->kind<MULIB_NB_MSG_STDOUT||m->kind>MULIB_NB_MSG_DONE)return 0;r->next_sequence++;if(m->kind==MULIB_NB_MSG_ERROR)r->state=MULIB_NB_RUN_FAILED;else if(m->kind==MULIB_NB_MSG_DONE)r->state=MULIB_NB_RUN_DONE;return 1;}
static inline int mulib_nb_execute_cancel(mulib_nb_run *r,const mulib_nb_runtime_ops *op){if(!r||(r->state!=MULIB_NB_RUN_QUEUED&&r->state!=MULIB_NB_RUN_RUNNING))return 0;if(r->state==MULIB_NB_RUN_RUNNING&&(!op||!op->interrupt||!op->interrupt(op->user,r->request)))return 0;r->state=MULIB_NB_RUN_CANCELLED;return 1;}
#endif
