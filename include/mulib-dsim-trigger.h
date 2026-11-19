/* MuLIB / Amb3r: dsim-trigger. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_TRIGGER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_TRIGGER_H
#define MULIB_DSIM_TRIGGER_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-resolve.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned mulib_dsim_trigger(unsigned old_value,unsigned new_value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_TRIGGER) && !defined(MULIB_DSIM_TRIGGER_H_IMPLEMENTED)
#define MULIB_DSIM_TRIGGER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_dsim_trigger(unsigned old_value,unsigned new_value){if(old_value>MA_LOGIC_Z||new_value>MA_LOGIC_Z||old_value==new_value)return 0;unsigned flags=1;if((old_value==MA_LOGIC_0&&new_value!=MA_LOGIC_0)||(new_value==MA_LOGIC_1&&old_value!=MA_LOGIC_1))flags|=2;if((old_value==MA_LOGIC_1&&new_value!=MA_LOGIC_1)||(new_value==MA_LOGIC_0&&old_value!=MA_LOGIC_0))flags|=4;return flags;}
#ifdef __cplusplus
}
#endif
#endif
