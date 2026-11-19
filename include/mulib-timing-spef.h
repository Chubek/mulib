/* MuLIB / Amb3r: timing-spef. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_SPEF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_SPEF_H
#define MULIB_TIMING_SPEF_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_spef_net(FILE *f,const char *net,const char *const *names,const double *cap,size_t n,const ma_edge *res,size_t m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_SPEF) && !defined(MULIB_TIMING_SPEF_H_IMPLEMENTED)
#define MULIB_TIMING_SPEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_spef_net(FILE *f,const char *net,const char *const *names,const double *cap,size_t n,const ma_edge *res,size_t m){if(!f||!net||strpbrk(net," \t\r\n")||(!names&&n)||(!cap&&n)||(!res&&m))return 0;double total=0;for(size_t i=0;i<n;i++){if(!names[i]||strpbrk(names[i]," \t\r\n")||cap[i]<0||!isfinite(cap[i]))return 0;total+=cap[i];}for(size_t i=0;i<m;i++)if(res[i].from>=n||res[i].to>=n||res[i].weight<0||!isfinite(res[i].weight))return 0;if(fprintf(f,"*D_NET %s %.12g\n*CAP\n",net,total)<0)return 0;for(size_t i=0;i<n;i++)if(fprintf(f,"%zu %s %.12g\n",i+1,names[i],cap[i])<0)return 0;if(fputs("*RES\n",f)<0)return 0;for(size_t i=0;i<m;i++)if(fprintf(f,"%zu %s %s %.12g\n",i+1,names[res[i].from],names[res[i].to],res[i].weight)<0)return 0;return fputs("*END\n",f)>=0;}
#ifdef __cplusplus
}
#endif
#endif
