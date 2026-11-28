/* MuLIB browser staging addition: web-input
 * Implemented subset: Validate finite pointer coordinates, button and event kind.
 * Remaining: Keyboard, touch, pointer capture, event dispatch and default actions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_INPUT_H
#define MULIB_WEB_INPUT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double x,y;int button;unsigned kind;} mulib_web_input_pointer;
static inline int mulib_web_input_valid(mulib_web_input_pointer e){return isfinite(e.x)&&isfinite(e.y)&&e.button>=-1&&e.button<=4&&e.kind<=3;}
#endif /* MULIB_WEB_INPUT_H */
