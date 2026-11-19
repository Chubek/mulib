/* MuLIB / Amb3r: spice-params. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_PARAMS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_PARAMS_H
#define MULIB_SPICE_PARAMS_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_params_number(const char *s,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_PARAMS) && !defined(MULIB_SPICE_PARAMS_H_IMPLEMENTED)
#define MULIB_SPICE_PARAMS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_params_number(const char *s,double *out){if(!s||!out)return 0;char *end;errno=0;double v=strtod(s,&end);if(end==s||errno||!isfinite(v))return 0;char suffix[8];size_t n=0;while(isalpha((unsigned char)*end)){if(n+1==sizeof suffix)return 0;suffix[n++]=(char)tolower((unsigned char)*end++);}suffix[n]=0;while(isspace((unsigned char)*end))end++;if(*end)return 0;double m=1;if(!strcmp(suffix,"t"))m=1e12;else if(!strcmp(suffix,"g"))m=1e9;else if(!strcmp(suffix,"meg"))m=1e6;else if(!strcmp(suffix,"k"))m=1e3;else if(!strcmp(suffix,"m"))m=1e-3;else if(!strcmp(suffix,"u"))m=1e-6;else if(!strcmp(suffix,"n"))m=1e-9;else if(!strcmp(suffix,"p"))m=1e-12;else if(!strcmp(suffix,"f"))m=1e-15;else if(n)return 0;v*=m;if(!isfinite(v))return 0;*out=v;return 1;}
#ifdef __cplusplus
}
#endif
#endif
