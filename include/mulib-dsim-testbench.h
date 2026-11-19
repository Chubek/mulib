/* MuLIB / Amb3r: dsim-testbench. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_TESTBENCH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_TESTBENCH_H
#define MULIB_DSIM_TESTBENCH_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_dsim_testbench_compare(const unsigned *actual,const unsigned *expected,size_t n,size_t *first);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_TESTBENCH) && !defined(MULIB_DSIM_TESTBENCH_H_IMPLEMENTED)
#define MULIB_DSIM_TESTBENCH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_dsim_testbench_compare(const unsigned *actual,const unsigned *expected,size_t n,size_t *first){if((!actual&&n)||(!expected&&n))return MA_NONE;size_t count=0;if(first)*first=MA_NONE;for(size_t i=0;i<n;i++)if(expected[i]<=3&&actual[i]!=expected[i]){if(!count&&first)*first=i;count++;}return count;}
#ifdef __cplusplus
}
#endif
#endif
