/* MuLIB / Amb3r: spice-rawfile. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_RAWFILE in exactly one implementation translation unit.
 * Profile: Real ASCII transient raw writer; not full rawfile reader or binary/complex interchange.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_RAWFILE_H
#define MULIB_SPICE_RAWFILE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_rawfile_write(FILE *f,const char *const *names,size_t vars,const double *values,size_t points);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_RAWFILE) && !defined(MULIB_SPICE_RAWFILE_H_IMPLEMENTED)
#define MULIB_SPICE_RAWFILE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_rawfile_write(FILE *f,const char *const *names,size_t vars,const double *values,size_t points){size_t count;if(!f||!names||!vars||!ma_mul(vars,points,&count)||!ma_finite(values,count))return 0;for(size_t i=0;i<vars;i++)if(!names[i]||strpbrk(names[i],"\r\n\t "))return 0;if(fprintf(f,"Title: Amb3r\nPlotname: Transient Analysis\nFlags: real\nNo. Variables: %zu\nNo. Points: %zu\nVariables:\n",vars,points)<0)return 0;for(size_t i=0;i<vars;i++)if(fprintf(f,"\t%zu\t%s\t%s\n",i,names[i],i?"voltage":"time")<0)return 0;if(fputs("Values:\n",f)<0)return 0;for(size_t i=0;i<points;i++)for(size_t j=0;j<vars;j++)if((j?fprintf(f,"\t%.17g\n",values[i*vars+j]):fprintf(f," %zu\t%.17g\n",i,values[i*vars]))<0)return 0;return !ferror(f);}
#ifdef __cplusplus
}
#endif
#endif
