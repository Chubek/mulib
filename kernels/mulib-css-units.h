/* MuLIB browser staging addition: css-units
 * Implemented subset: Convert px, in, cm, mm, pt, pc, em, rem, vw, vh and percent to px.
 * Remaining: Font-relative units, viewport variants, container units and dimensional analysis.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_UNITS_H
#define MULIB_CSS_UNITS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_units_px(double x,muba_span unit,double em,double rem,double vw,double vh,double base,double *out){if(!out||!isfinite(x))return 0;const char *v[]={"px","in","cm","mm","pt","pc","em","rem","vw","vh","%"};double f[]={1,96,96/2.54,96/25.4,96/72.0,16,em,rem,vw/100,vh/100,base/100};for(size_t i=0;i<11;i++)if(muba_ieq(unit,muba_str(v[i]))&&isfinite(x*f[i])){*out=x*f[i];return 1;}return 0;}
#endif /* MULIB_CSS_UNITS_H */
