/* MuLIB browser staging addition: html-form
 * Implemented subset: Required text and bounded numeric range validation helpers.
 * Remaining: Form ownership, submission, successful controls, validation events.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_FORM_H
#define MULIB_HTML_FORM_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_form_required(muba_span value,int required){return muba_span_ok(value)&&(!required||value.len!=0);}
static inline int mulib_html_form_range(double n,double lo,double hi){return isfinite(n)&&!isnan(lo)&&!isnan(hi)&&lo<=hi&&n>=lo&&n<=hi;}
#endif /* MULIB_HTML_FORM_H */
