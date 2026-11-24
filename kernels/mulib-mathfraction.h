/** @file mulib-mathfraction.h
 * @brief Compute stacked fraction baseline metrics from numerator and denominator boxes.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MATHFRACTION once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mathfraction.md for scope and limitations.
 */
#ifndef MULIB_MATHFRACTION_H
#define MULIB_MATHFRACTION_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mathfraction_metrics {double width,height,depth,rule_y;} mulib_mathfraction_metrics_t;
int mulib_mathfraction_stack(double num_width,double num_height,double den_width,double den_depth,double gap,double rule,mulib_mathfraction_metrics_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATHFRACTION_H */
#if defined(MULIB_IMPL_MATHFRACTION) && !defined(MULIB_MATHFRACTION_H_IMPLEMENTED)
#define MULIB_MATHFRACTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathfraction_stack(double nw,double nh,double dw,double dd,double gap,double rule,mulib_mathfraction_metrics_t *out){if(!out||!isfinite(nw)||!isfinite(nh)||!isfinite(dw)||!isfinite(dd)||!isfinite(gap)||!isfinite(rule)||nw<0||nh<0||dw<0||dd<0||gap<0||rule<0)return 0;double width=fmax(nw,dw),height=nh+gap+rule/2,depth=dd+gap+rule/2;if(!isfinite(width+height+depth))return 0;*out=(mulib_mathfraction_metrics_t){width,height,depth,0};return 1;}
#ifdef __cplusplus
}
#endif
#endif
