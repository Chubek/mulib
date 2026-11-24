/* MuLIB: canvas-selection. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_SELECTION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_SELECTION_H
#define MULIB_CANVAS_SELECTION_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_canvas_selection(const mulib_geom2_box *boxes,size_t count,mulib_geom2_box selection,int contained,size_t *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_SELECTION) && !defined(MULIB_CANVAS_SELECTION_H_IMPLEMENTED)
#define MULIB_CANVAS_SELECTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_canvas_selection(const mulib_geom2_box *boxes,size_t count,mulib_geom2_box selection,int contained,size_t *out,size_t cap){if((!boxes&&count)||!mulib_geom2_box_valid(selection))return MULIB_STORAGE_NONE;size_t n=0;for(size_t i=0;i<count;i++){if(!mulib_geom2_box_valid(boxes[i]))return MULIB_STORAGE_NONE;int hit=contained?boxes[i].lo.x>=selection.lo.x&&boxes[i].hi.x<=selection.hi.x&&boxes[i].lo.y>=selection.lo.y&&boxes[i].hi.y<=selection.hi.y:mulib_geom2_box_overlap(boxes[i],selection);if(hit){if(out&&n<cap)out[n]=i;n++;}}return n;}
#ifdef __cplusplus
}
#endif
#endif
