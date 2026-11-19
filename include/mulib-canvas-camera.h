/* MuLIB / Amb3r: canvas-camera. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_CAMERA in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_CAMERA_H
#define MULIB_CANVAS_CAMERA_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {ma_point origin,viewport;double zoom;} ma_canvas_camera;
ma_point mulib_canvas_camera_to_screen(ma_canvas_camera c,ma_point p);
int mulib_canvas_camera_to_world(ma_canvas_camera c,ma_point p,ma_point *out);
int mulib_canvas_camera_zoom(ma_canvas_camera *c,ma_point anchor,double factor,double min,double max);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_CAMERA) && !defined(MULIB_CANVAS_CAMERA_H_IMPLEMENTED)
#define MULIB_CANVAS_CAMERA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
ma_point mulib_canvas_camera_to_screen(ma_canvas_camera c,ma_point p){return (ma_point){(p.x-c.origin.x)*c.zoom+c.viewport.x,(p.y-c.origin.y)*c.zoom+c.viewport.y};}
int mulib_canvas_camera_to_world(ma_canvas_camera c,ma_point p,ma_point *out){if(!out||c.zoom<=0||!isfinite(c.zoom))return 0;*out=(ma_point){(p.x-c.viewport.x)/c.zoom+c.origin.x,(p.y-c.viewport.y)/c.zoom+c.origin.y};return ma_point_valid(*out);}
int mulib_canvas_camera_zoom(ma_canvas_camera *c,ma_point anchor,double factor,double min,double max){if(!c||factor<=0||min<=0||max<min||!isfinite(factor+min+max))return 0;ma_point p;if(!mulib_canvas_camera_to_world(*c,anchor,&p))return 0;double z=MA_MAX(min,MA_MIN(max,c->zoom*factor));c->origin=(ma_point){p.x-(anchor.x-c->viewport.x)/z,p.y-(anchor.y-c->viewport.y)/z};c->zoom=z;return 1;}
#ifdef __cplusplus
}
#endif
#endif
