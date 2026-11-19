/* MuLIB / Amb3r: hdl-timescale. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_TIMESCALE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_TIMESCALE_H
#define MULIB_HDL_TIMESCALE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_timescale_parse(const char *s,uint64_t *fs);
int mulib_hdl_timescale_convert(uint64_t ticks,uint64_t from,uint64_t to,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_TIMESCALE) && !defined(MULIB_HDL_TIMESCALE_H_IMPLEMENTED)
#define MULIB_HDL_TIMESCALE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_timescale_parse(const char *s,uint64_t *fs){if(!s||!fs)return 0;char *end;errno=0;unsigned long long v=strtoull(s,&end,10);if(errno||end==s||!(v==1||v==10||v==100))return 0;while(isspace((unsigned char)*end))end++;uint64_t m;if(!strcmp(end,"s"))m=UINT64_C(1000000000000000);else if(!strcmp(end,"ms"))m=UINT64_C(1000000000000);else if(!strcmp(end,"us"))m=UINT64_C(1000000000);else if(!strcmp(end,"ns"))m=1000000;else if(!strcmp(end,"ps"))m=1000;else if(!strcmp(end,"fs"))m=1;else return 0;*fs=v*m;return 1;}
int mulib_hdl_timescale_convert(uint64_t ticks,uint64_t from,uint64_t to,uint64_t *out){if(!from||!to||!out||ticks>UINT64_MAX/from)return 0;uint64_t x=ticks*from;if(x%to)return 0;*out=x/to;return 1;}
#ifdef __cplusplus
}
#endif
#endif
