/* MuLIB / Amb3r: canvas-selection. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_SELECTION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_SELECTION_H
#define MULIB_CANVAS_SELECTION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_canvas_selection(const ma_box *boxes,size_t count,ma_box selection,int contained,size_t *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_SELECTION) && !defined(MULIB_CANVAS_SELECTION_H_IMPLEMENTED)
#define MULIB_CANVAS_SELECTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_canvas_selection(const ma_box *boxes,size_t count,ma_box selection,int contained,size_t *out,size_t cap){if((!boxes&&count)||!ma_box_valid(selection))return MA_NONE;size_t n=0;for(size_t i=0;i<count;i++){if(!ma_box_valid(boxes[i]))return MA_NONE;int hit=contained?boxes[i].lo.x>=selection.lo.x&&boxes[i].hi.x<=selection.hi.x&&boxes[i].lo.y>=selection.lo.y&&boxes[i].hi.y<=selection.hi.y:ma_box_hit(boxes[i],selection);if(hit){if(out&&n<cap)out[n]=i;n++;}}return n;}
#ifdef __cplusplus
}
#endif
#endif
