/** @file mulib-color-primaries.h
 * 3x3 color matrices and D65 Rec.709/Rec.2020/P3 matrices.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COLOR_PRIMARIES_H
#define MULIB_COLOR_PRIMARIES_H
#include "mulib-img-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {double m[9];} mulib_color_mat3;
static inline mulib_color_mat3 mulib_color_primaries_mul(mulib_color_mat3 a,mulib_color_mat3 b){mulib_color_mat3 c={{0}};for(int i=0;i<3;i++)for(int j=0;j<3;j++)for(int k=0;k<3;k++)c.m[3*i+j]+=a.m[3*i+k]*b.m[3*k+j];return c;}
static inline bool mulib_color_primaries_inverse(mulib_color_mat3 a,mulib_color_mat3 *out){if(!out)return false;double *m=a.m;for(int i=0;i<9;i++)if(!isfinite(m[i]))return false;mulib_color_mat3 z={{m[4]*m[8]-m[5]*m[7],m[2]*m[7]-m[1]*m[8],m[1]*m[5]-m[2]*m[4],m[5]*m[6]-m[3]*m[8],m[0]*m[8]-m[2]*m[6],m[2]*m[3]-m[0]*m[5],m[3]*m[7]-m[4]*m[6],m[1]*m[6]-m[0]*m[7],m[0]*m[4]-m[1]*m[3]}};double d=m[0]*z.m[0]+m[1]*z.m[3]+m[2]*z.m[6];if(!isfinite(d)||fabs(d)<1e-15)return false;for(int i=0;i<9;i++){z.m[i]/=d;if(!isfinite(z.m[i]))return false;}*out=z;return true;}
static inline void mulib_color_primaries_apply(mulib_color_mat3 m,const double a[3],double o[3]){double z[3];for(int i=0;i<3;i++)z[i]=m.m[3*i]*a[0]+m.m[3*i+1]*a[1]+m.m[3*i+2]*a[2];memcpy(o,z,sizeof z);}
static inline mulib_color_mat3 mulib_color_primaries_rec709_xyz(void){mulib_color_mat3 m={{.4123908,.3575843,.1804808,.2126390,.7151687,.0721923,.0193308,.1191948,.9505322}};return m;}
static inline mulib_color_mat3 mulib_color_primaries_rec2020_xyz(void){mulib_color_mat3 m={{.636958,.144617,.168881,.262700,.677998,.059302,0,.028073,1.060985}};return m;}
static inline mulib_color_mat3 mulib_color_primaries_p3_xyz(void){mulib_color_mat3 m={{.486571,.265668,.198217,.228975,.691739,.079287,0,.045113,1.043944}};return m;}
#ifdef __cplusplus
}
#endif
#endif
