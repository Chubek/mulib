/** @file mulib-texbreak.h
 * @brief Optimal dynamic-programming line breaks over fixed widths and penalties.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXBREAK once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texbreak.md for scope and limitations.
 */
#ifndef MULIB_TEXBREAK_H
#define MULIB_TEXBREAK_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texbreak_result { size_t *breaks;size_t count; } mulib_texbreak_result_t;
int mulib_texbreak_solve(const double *widths,const int *penalties,size_t count,double line_width,mulib_texbreak_result_t *out);
void mulib_texbreak_destroy(mulib_texbreak_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXBREAK_H */
#if defined(MULIB_IMPL_TEXBREAK) && !defined(MULIB_TEXBREAK_H_IMPLEMENTED)
#define MULIB_TEXBREAK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texbreak_solve(const double *w,const int *pen,size_t n,double target,mulib_texbreak_result_t *out){if(!out||(!w&&n)||(!pen&&n)||!isfinite(target)||target<=0||n>4096)return 0;double *dp=(double*)malloc((n+1)*sizeof(*dp));size_t *prev=(size_t*)malloc((n+1)*sizeof(*prev));if(!dp||!prev){free(dp);free(prev);return 0;}for(size_t i=0;i<=n;i++){dp[i]=INFINITY;prev[i]=0;}dp[0]=0;
 for(size_t i=0;i<n;i++){if(!isfinite(dp[i]))continue;double sum=0;for(size_t j=i+1;j<=n;j++){if(!isfinite(w[j-1])||w[j-1]<0){free(dp);free(prev);return 0;}sum+=w[j-1];if(sum>target&&j>i+1)break;double slack=target-sum;double cost=(j==n?0:slack*slack)+(j==n?0:(double)pen[j-1]*pen[j-1]);if(sum>target)cost+=1000000;double cand=dp[i]+cost;if(cand<dp[j]){dp[j]=cand;prev[j]=i;}}}
 if(!isfinite(dp[n])){free(dp);free(prev);return 0;}size_t lines=0;for(size_t j=n;j>0;j=prev[j])lines++;size_t *arr=(size_t*)malloc((lines?lines:1)*sizeof(*arr));if(!arr){free(dp);free(prev);return 0;}size_t k=lines;for(size_t j=n;j>0;j=prev[j])arr[--k]=j;out->breaks=arr;out->count=lines;free(dp);free(prev);return 1;}
void mulib_texbreak_destroy(mulib_texbreak_result_t *out){if(out){free(out->breaks);memset(out,0,sizeof(*out));}}
#ifdef __cplusplus
}
#endif
#endif
