/* MuLIB / Amb3r: rf-touchstone. C11 single-header kernel.
 * Define MULIB_IMPL_RF_TOUCHSTONE in exactly one implementation translation unit.
 * Profile: Touchstone 1 two-port S2P, one complete record per line; not all Touchstone variants.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_TOUCHSTONE_H
#define MULIB_RF_TOUCHSTONE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_touchstone_cb)(void *,double,const ma_complex *,double);
int mulib_rf_touchstone_read_s2p(FILE *f,ma_touchstone_cb callback,void *ctx,double *reference);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_TOUCHSTONE) && !defined(MULIB_RF_TOUCHSTONE_H_IMPLEMENTED)
#define MULIB_RF_TOUCHSTONE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_touchstone_read_s2p(FILE *f,ma_touchstone_cb callback,void *ctx,double *reference){if(!f||!callback||!reference)return 0;char line[4096],unit[8],param[8],form[8],r[8];double factor=1e9,z0=50;unsigned mode=1;int has=0;while(fgets(line,sizeof line,f)){size_t len=strlen(line);if(len==sizeof line-1&&line[len-1]!='\n')return 0;char *p=line,*comment=strchr(p,'!');if(comment)*comment=0;while(isspace((unsigned char)*p))p++;if(!*p)continue;if(*p=='#'){if(sscanf(p+1,"%7s %7s %7s %7s %lf",unit,param,form,r,&z0)!=5||z0<=0||!isfinite(z0))return 0;for(char *x=unit;*x;x++)*x=(char)toupper((unsigned char)*x);for(char *x=form;*x;x++)*x=(char)toupper((unsigned char)*x);if(toupper((unsigned char)param[0])!='S'||param[1]||toupper((unsigned char)r[0])!='R'||r[1])return 0;if(!strcmp(unit,"HZ"))factor=1;else if(!strcmp(unit,"KHZ"))factor=1e3;else if(!strcmp(unit,"MHZ"))factor=1e6;else if(!strcmp(unit,"GHZ"))factor=1e9;else return 0;if(!strcmp(form,"RI"))mode=0;else if(!strcmp(form,"MA"))mode=1;else if(!strcmp(form,"DB"))mode=2;else return 0;continue;}double v[9];for(size_t i=0;i<9;i++){char *end;v[i]=strtod(p,&end);if(end==p||!isfinite(v[i]))return 0;p=end;}while(isspace((unsigned char)*p))p++;if(*p||v[0]<0)return 0;ma_complex s[4];for(size_t i=0;i<4;i++){double a=v[2*i+1],b=v[2*i+2];if(mode){a=mode==2?pow(10,a/20):a;if(a<0)return 0;b*=MA_PI/180;s[i]=(ma_complex){a*cos(b),a*sin(b)};}else s[i]=(ma_complex){a,b};}if(!callback(ctx,v[0]*factor,s,z0))return 0;has=1;}*reference=z0;return has&&!ferror(f);}
#ifdef __cplusplus
}
#endif
#endif
