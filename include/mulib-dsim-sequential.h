/* MuLIB / Amb3r: dsim-sequential. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_SEQUENTIAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_SEQUENTIAL_H
#define MULIB_DSIM_SEQUENTIAL_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-resolve.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned mulib_dsim_sequential_dff(unsigned old_clock,unsigned clock,unsigned d,unsigned q,unsigned reset,unsigned reset_value);
unsigned mulib_dsim_sequential_latch(unsigned enable,unsigned d,unsigned prior);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_SEQUENTIAL) && !defined(MULIB_DSIM_SEQUENTIAL_H_IMPLEMENTED)
#define MULIB_DSIM_SEQUENTIAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_dsim_sequential_dff(unsigned old_clock,unsigned clock,unsigned d,unsigned q,unsigned reset,unsigned reset_value){if(reset==MA_LOGIC_1)return reset_value;if(reset!=MA_LOGIC_0)return MA_LOGIC_X;if(old_clock==MA_LOGIC_0&&clock==MA_LOGIC_1)return d==MA_LOGIC_Z?MA_LOGIC_X:d;return q;}
unsigned mulib_dsim_sequential_latch(unsigned enable,unsigned d,unsigned prior){if(enable==MA_LOGIC_1)return d==MA_LOGIC_Z?MA_LOGIC_X:d;if(enable==MA_LOGIC_0)return prior;return d==prior?prior:MA_LOGIC_X;}
#ifdef __cplusplus
}
#endif
#endif
