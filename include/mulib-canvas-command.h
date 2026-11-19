/* MuLIB / Amb3r: canvas-command. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_COMMAND in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_COMMAND_H
#define MULIB_CANVAS_COMMAND_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned kind;uint32_t rgba;ma_point a,b;double width;} ma_canvas_command;
typedef struct {ma_canvas_command *v;size_t n,cap;} ma_canvas_commands;
typedef int (*ma_canvas_callback)(void *,const ma_canvas_command *);
int mulib_canvas_command_push(ma_canvas_commands *c,ma_canvas_command command);
int mulib_canvas_command_flush(const ma_canvas_commands *c,ma_canvas_callback cb,void *ctx);
void mulib_canvas_command_free(ma_canvas_commands *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_COMMAND) && !defined(MULIB_CANVAS_COMMAND_H_IMPLEMENTED)
#define MULIB_CANVAS_COMMAND_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_canvas_command_push(ma_canvas_commands *c,ma_canvas_command command){if(!c||command.kind>3||!ma_point_valid(command.a)||!ma_point_valid(command.b)||!isfinite(command.width)||command.width<0)return 0;return ma_push((void**)&c->v,&c->n,&c->cap,sizeof command,&command);}
int mulib_canvas_command_flush(const ma_canvas_commands *c,ma_canvas_callback cb,void *ctx){if(!c||!cb)return 0;for(size_t i=0;i<c->n;i++)if(!cb(ctx,c->v+i))return 0;return 1;}
void mulib_canvas_command_free(ma_canvas_commands *c){if(c){free(c->v);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
