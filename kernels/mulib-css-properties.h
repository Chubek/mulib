/* MuLIB browser staging addition: css-properties
 * Implemented subset: Property metadata lookup for a small common property set.
 * Remaining: Generated full property registry, grammar, inherited and initial values.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_PROPERTIES_H
#define MULIB_CSS_PROPERTIES_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {const char *name,*initial;int inherited;} mulib_css_properties_entry;
static inline const mulib_css_properties_entry *mulib_css_properties_get(muba_span name){static const mulib_css_properties_entry v[]={{"color","canvastext",1},{"display","inline",0},{"width","auto",0},{"height","auto",0},{"font-size","medium",1},{"visibility","visible",1},{"opacity","1",0},{"position","static",0},{"overflow","visible",0},{"box-sizing","content-box",0}};for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_ieq(name,muba_str(v[i].name)))return v+i;return NULL;}
#endif /* MULIB_CSS_PROPERTIES_H */
