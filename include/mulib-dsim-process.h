/* MuLIB / Amb3r: dsim-process. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_PROCESS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_PROCESS_H
#define MULIB_DSIM_PROCESS_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-engine.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const size_t *sensitivity;size_t sensitivity_count;int (*run)(void *,ma_sim_engine *,size_t);void *context;} ma_sim_process;
int mulib_dsim_process_notify(const ma_sim_process *p,size_t n,size_t changed,ma_sim_engine *engine);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_PROCESS) && !defined(MULIB_DSIM_PROCESS_H_IMPLEMENTED)
#define MULIB_DSIM_PROCESS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_process_notify(const ma_sim_process *p,size_t n,size_t changed,ma_sim_engine *engine){if((!p&&n)||!engine)return 0;for(size_t i=0;i<n;i++){int hit=0;for(size_t j=0;j<p[i].sensitivity_count;j++)if(p[i].sensitivity[j]==changed)hit=1;if(hit&&(!p[i].run||!p[i].run(p[i].context,engine,changed)))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
