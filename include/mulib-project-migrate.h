/** @file mulib-project-migrate.h
 * Ordered monotonic schema migration execution with ambiguity rejection.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Each step must be transactional; earlier successful steps remain committed.
 */
#ifndef MULIB_PROJECT_MIGRATE_H
#define MULIB_PROJECT_MIGRATE_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef bool (*mulib_project_migrate_fn)(void*);
typedef struct {uint32_t from,to;mulib_project_migrate_fn run;} mulib_project_migrate_step;
/* Validate the entire unambiguous route before invoking any callback. A failed
 * callback must roll back its own step; earlier successful steps stay committed.
 * Callbacks must not mutate s/version or reenter this migration. */
static inline bool mulib_project_migrate(const mulib_project_migrate_step *s,size_t n,uint32_t *version,uint32_t target,void *ctx){
 if(!version||(!s&&n)||n>SIZE_MAX/sizeof *s||*version>target)return false;
 uint32_t v=*version;
 while(v<target){
  size_t k=SIZE_MAX;
  for(size_t i=0;i<n;i++)if(s[i].from==v&&s[i].to>v&&s[i].to<=target){if(k!=SIZE_MAX)return false;k=i;}
  if(k==SIZE_MAX||!s[k].run)return false;
  v=s[k].to;
 }
 while(*version<target){
  size_t k=0;while(k<n&&!(s[k].from==*version&&s[k].to>*version&&s[k].to<=target))k++;
  if(k==n||!s[k].run(ctx))return false;
  *version=s[k].to;
 }
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
