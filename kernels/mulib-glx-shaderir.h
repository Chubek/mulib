/* MuLIB graphical extension: glx-shaderir. C11, header-only. */
#ifndef MULIB_GLX_SHADERIR_H
#define MULIB_GLX_SHADERIR_H
#include "mulib-glx-core.h"
typedef enum{MGX_SH_CONST,MGX_SH_INPUT,MGX_SH_ADD,MGX_SH_MUL,MGX_SH_SUB,MGX_SH_DIV,MGX_SH_MIN,MGX_SH_MAX,MGX_SH_CLAMP,MGX_SH_SIN}mgx_shader_op;
typedef struct{mgx_shader_op op;uint16_t a,b;float immediate;}mgx_shader_instr;
typedef struct{mgx_shader_instr *v;size_t n,cap;uint16_t outputs[4];}mulib_glx_shaderir_t;
typedef mgx_shader_op mulib_glx_shader_op_t;
typedef mgx_shader_instr mulib_glx_shader_instruction_t;
static inline int
mgx_shader_instruction_valid(mgx_shader_instr i,size_t index)
{
 return i.op>=MGX_SH_CONST&&i.op<=MGX_SH_SIN&&isfinite(i.immediate)&&
        (i.op!=MGX_SH_INPUT||i.a<16)&&
        (i.op<MGX_SH_ADD||i.a<index)&&
        (i.op<MGX_SH_ADD||i.op>MGX_SH_MAX||i.b<index);
}
/* Returns the register index or -1. Only backward references are accepted. */
static inline int
mulib_glx_shaderir_add(mulib_glx_shaderir_t *s,mgx_shader_instr i)
{
 if(!s||s->n>s->cap||(s->cap&&!s->v)||s->n>=65535||
    !mgx_shader_instruction_valid(i,s->n)||
    !mgx_grow((void**)&s->v,&s->cap,s->n+1,sizeof i))return -1;
 s->v[s->n]=i;
 return (int)s->n++;
}
static inline int
mulib_glx_shaderir_validate(const mulib_glx_shaderir_t *s)
{
 if(!s||!s->v||!s->n||s->n>s->cap||s->n>65535)return 0;
 for(int i=0;i<4;i++)if(s->outputs[i]>=s->n)return 0;
 for(size_t k=0;k<s->n;k++)
  if(!mgx_shader_instruction_valid(s->v[k],k))return 0;
 return 1;
}
static inline void mulib_glx_shaderir_free(mulib_glx_shaderir_t *s)
{
 if(s){free(s->v);memset(s,0,sizeof *s);}
}
#endif
