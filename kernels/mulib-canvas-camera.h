/* MuLIB: canvas-camera. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_CAMERA in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_CAMERA_H
#define MULIB_CANVAS_CAMERA_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_geom2_point origin,viewport;double zoom;} mulib_canvas_camera;
mulib_geom2_point mulib_canvas_camera_to_screen(mulib_canvas_camera c,mulib_geom2_point p);
int mulib_canvas_camera_to_world(mulib_canvas_camera c,mulib_geom2_point p,mulib_geom2_point *out);
int mulib_canvas_camera_zoom(mulib_canvas_camera *c,mulib_geom2_point anchor,double factor,double min,double max);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_CAMERA) && !defined(MULIB_CANVAS_CAMERA_H_IMPLEMENTED)
#define MULIB_CANVAS_CAMERA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_geom2_point mulib_canvas_camera_to_screen(mulib_canvas_camera c,mulib_geom2_point p){return (mulib_geom2_point){(p.x-c.origin.x)*c.zoom+c.viewport.x,(p.y-c.origin.y)*c.zoom+c.viewport.y};}
int mulib_canvas_camera_to_world(mulib_canvas_camera c,mulib_geom2_point p,mulib_geom2_point *out){if(!out||c.zoom<=0||!isfinite(c.zoom))return 0;*out=(mulib_geom2_point){(p.x-c.viewport.x)/c.zoom+c.origin.x,(p.y-c.viewport.y)/c.zoom+c.origin.y};return mulib_geom2_point_valid(*out);}
int mulib_canvas_camera_zoom(mulib_canvas_camera *c,mulib_geom2_point anchor,double factor,double min,double max){if(!c||factor<=0||min<=0||max<min||!isfinite(factor+min+max))return 0;mulib_geom2_point p;if(!mulib_canvas_camera_to_world(*c,anchor,&p))return 0;double z=MULIB_STORAGE_MAX(min,MULIB_STORAGE_MIN(max,c->zoom*factor));c->origin=(mulib_geom2_point){p.x-(anchor.x-c->viewport.x)/z,p.y-(anchor.y-c->viewport.y)/z};c->zoom=z;return 1;}
#ifdef __cplusplus
}
#endif
#endif
