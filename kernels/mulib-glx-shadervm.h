/* MuLIB graphical extension: glx-shadervm. C11, header-only. */
#ifndef MULIB_GLX_SHADERVM_H
#define MULIB_GLX_SHADERVM_H
#include "mulib-glx-shaderir.h"
static inline float
mgx_shader_eval(mgx_shader_instr i,float a,float b,const float in[16])
{
 switch(i.op){
 case MGX_SH_CONST:return i.immediate;
 case MGX_SH_INPUT:return in&&i.a<16?in[i.a]:NAN;
 case MGX_SH_ADD:return a+b;
 case MGX_SH_MUL:return a*b;
 case MGX_SH_SUB:return a-b;
 case MGX_SH_DIV:return b?a/b:0;
 case MGX_SH_MIN:return fminf(a,b);
 case MGX_SH_MAX:return fmaxf(a,b);
 case MGX_SH_CLAMP:return fminf(1,fmaxf(0,a));
 case MGX_SH_SIN:return sinf(a);
 }
 return NAN;
}
/* Scratch contains at least s->n floats and must not overlap output or IR.
 * Inputs are snapshotted, so in may alias scratch or output. On failure scratch
 * is unspecified, but output is unchanged. The legacy DIV-by-zero result is 0. */
static inline int
mulib_glx_shadervm_run(const mulib_glx_shaderir_t *s,const float in[16],
                       float out[4],float *scratch,size_t n)
{
 float inputs[16],result[4];
 if(!in||!out||!scratch||!mulib_glx_shaderir_validate(s)||n<s->n)return 0;
 memcpy(inputs,in,sizeof inputs);
 for(size_t k=0;k<s->n;k++){
  mgx_shader_instr i=s->v[k];
  if(i.op==MGX_SH_INPUT&&!isfinite(inputs[i.a]))return 0;
  scratch[k]=mgx_shader_eval(i,i.op>=MGX_SH_ADD?scratch[i.a]:0,
      i.op>=MGX_SH_ADD&&i.op<=MGX_SH_MAX?scratch[i.b]:0,inputs);
  if(!isfinite(scratch[k]))return 0;
 }
 for(int i=0;i<4;i++)result[i]=scratch[s->outputs[i]];
 memcpy(out,result,sizeof result);
 return 1;
}
#endif
