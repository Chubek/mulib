/* MuLIB graphical extension: glx-shaderjit. C11, header-only. */
#ifndef MULIB_GLX_SHADERJIT_H
#define MULIB_GLX_SHADERJIT_H
#include "mulib-glx-shadervm.h"
/* Portable specialization/constant folding. No executable machine-code emission. */
/* Output must be initialized (empty or owning). Replacement, including in-place
 * specialization, is transactional. No executable memory is allocated. */
static inline int
mulib_glx_shaderjit_specialize(const mulib_glx_shaderir_t *src,
                              mulib_glx_shaderir_t *out)
{
 mulib_glx_shaderir_t next={0};
 float in[16]={0};
 if(!out||!mulib_glx_shaderir_validate(src))return 0;
 for(size_t k=0;k<src->n;k++){
  mgx_shader_instr i=src->v[k];
  if(i.op>=MGX_SH_ADD&&next.v[i.a].op==MGX_SH_CONST&&
     (i.op>MGX_SH_MAX||next.v[i.b].op==MGX_SH_CONST)){
   float b=i.op<=MGX_SH_MAX?next.v[i.b].immediate:0;
   float v=mgx_shader_eval(i,next.v[i.a].immediate,b,in);
   if(isfinite(v))i=(mgx_shader_instr){MGX_SH_CONST,0,0,v};
  }
  if(mulib_glx_shaderir_add(&next,i)<0){
   mulib_glx_shaderir_free(&next);
   return 0;
  }
 }
 memcpy(next.outputs,src->outputs,sizeof next.outputs);
 mulib_glx_shaderir_free(out);
 *out=next;
 return 1;
}
#endif
