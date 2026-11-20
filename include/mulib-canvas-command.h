/* MuLIB: canvas-command. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_COMMAND in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_COMMAND_H
#define MULIB_CANVAS_COMMAND_H
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
typedef struct {unsigned kind;uint32_t rgba;mulib_geom2_point a,b;double width;} mulib_canvas_command;
typedef struct {mulib_canvas_command *v;size_t n,cap;} mulib_canvas_commands;
typedef int (*mulib_canvas_callback)(void *,const mulib_canvas_command *);
int mulib_canvas_command_push(mulib_canvas_commands *c,mulib_canvas_command command);
int mulib_canvas_command_flush(const mulib_canvas_commands *c,mulib_canvas_callback cb,void *ctx);
void mulib_canvas_command_free(mulib_canvas_commands *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_COMMAND) && !defined(MULIB_CANVAS_COMMAND_H_IMPLEMENTED)
#define MULIB_CANVAS_COMMAND_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_canvas_command_push(mulib_canvas_commands *c,mulib_canvas_command command){if(!c||command.kind>3||!mulib_geom2_point_valid(command.a)||!mulib_geom2_point_valid(command.b)||!isfinite(command.width)||command.width<0)return 0;return mulib_storage_push((void**)&c->v,&c->n,&c->cap,sizeof command,&command);}
int mulib_canvas_command_flush(const mulib_canvas_commands *c,mulib_canvas_callback cb,void *ctx){if(!c||!cb)return 0;for(size_t i=0;i<c->n;i++)if(!cb(ctx,c->v+i))return 0;return 1;}
void mulib_canvas_command_free(mulib_canvas_commands *c){if(c){free(c->v);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
