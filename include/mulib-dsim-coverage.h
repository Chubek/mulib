/* MuLIB / Amb3r: dsim-coverage. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_COVERAGE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_COVERAGE_H
#define MULIB_DSIM_COVERAGE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t counts[4],toggles;unsigned previous,seen;} ma_sim_coverage;
void mulib_dsim_coverage_sample(ma_sim_coverage *c,unsigned value);
double mulib_dsim_coverage_binary(const ma_sim_coverage *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_COVERAGE) && !defined(MULIB_DSIM_COVERAGE_H_IMPLEMENTED)
#define MULIB_DSIM_COVERAGE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_dsim_coverage_sample(ma_sim_coverage *c,unsigned value){if(!c||value>3)return;if(c->seen&&c->previous!=value&&c->toggles!=UINT64_MAX)c->toggles++;if(c->counts[value]!=UINT64_MAX)c->counts[value]++;c->previous=value;c->seen=1;}
double mulib_dsim_coverage_binary(const ma_sim_coverage *c){return c?.5*((c->counts[0]!=0)+(c->counts[1]!=0)):NAN;}
#ifdef __cplusplus
}
#endif
#endif
