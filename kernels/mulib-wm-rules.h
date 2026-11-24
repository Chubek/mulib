/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_RULES_H
#define MULIB_WM_RULES_H
#include "mulib-wm-core.h"
typedef struct {const char *app,*title_prefix;uint64_t workspace,cell;unsigned set,clear;} mulib_wm_rule;
static inline int mulib_wm_rule_matches(const mulib_wm_rule *r,const mulib_wm_window *w){return r&&w&&(!r->app||!strcmp(r->app,w->app_id))&&(!r->title_prefix||!strncmp(w->title,r->title_prefix,strlen(r->title_prefix)));}
static inline size_t mulib_wm_rules_apply(mulib_wm_core *c,uint64_t id,const mulib_wm_rule *r,size_t n){mulib_wm_window *w=mulib_wm_find(c,id);if(!w||(!r&&n))return 0;size_t k=0;for(size_t i=0;i<n;i++)if(mulib_wm_rule_matches(r+i,w)){if(r[i].workspace)w->workspace=r[i].workspace;if(r[i].cell)w->cell=r[i].cell;w->flags=(w->flags|r[i].set)&~r[i].clear;k++;}if(k)c->revision++;return k;}
#endif
