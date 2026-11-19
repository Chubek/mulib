#include "mulib-glx-shaderjit.h"
#include "mulib-glx-gpu.h"
#include <assert.h>
#include <float.h>

static void
test_validation(void)
{
 mulib_glx_shaderir_t shader={0};
 mgx_shader_instr value={MGX_SH_CONST,0,0,2};
 assert(mulib_glx_shaderir_add(NULL,value)==-1);
 assert(!mulib_glx_shaderir_validate(NULL));
 assert(!mulib_glx_shaderir_validate(&shader));
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_ADD,0,0,0})==-1);
 assert(mulib_glx_shaderir_add(&shader,value)==0);
 assert(mulib_glx_shaderir_validate(&shader));
 shader.outputs[3]=1;
 assert(!mulib_glx_shaderir_validate(&shader));
 shader.outputs[3]=0;
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_INPUT,16,0,0})==-1);
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_CONST,0,0,NAN})==-1);
 assert(shader.n==1);
 shader.v[0].op=MGX_SH_ADD;
 assert(!mulib_glx_shaderir_validate(&shader));
 shader.v[0].op=MGX_SH_CONST;
 {
  size_t cap=shader.cap;
  shader.cap=0;
  assert(!mulib_glx_shaderir_validate(&shader));
  assert(mulib_glx_shaderir_add(&shader,value)==-1);
  shader.cap=cap;
 }
 assert(!mulib_glx_shaderjit_specialize(&shader,NULL));
 mulib_glx_shaderir_free(&shader);
 mulib_glx_shaderir_free(NULL);
}

static void
test_execution(void)
{
 mulib_glx_shaderir_t shader={0},folded={0};
 float inputs[16]={0},out[4]={9,9,9,9},expected[4],scratch[32];
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_CONST,0,0,2})==0);
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_CONST,0,0,3})==1);
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_ADD,0,1,0})==2);
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_INPUT,7,0,0})==3);
 assert(mulib_glx_shaderir_add(&shader,(mgx_shader_instr){MGX_SH_MUL,2,3,0})==4);
 for(int i=0;i<4;i++)shader.outputs[i]=4;
 inputs[7]=4;
 assert(!mulib_glx_shadervm_run(&shader,inputs,out,scratch,4));
 assert(out[0]==9);
 assert(mulib_glx_shadervm_run(&shader,inputs,out,scratch,32));
 assert(out[0]==20&&out[3]==20);
 memcpy(expected,out,sizeof expected);
 assert(mulib_glx_shaderjit_specialize(&shader,&folded));
 assert(folded.v[2].op==MGX_SH_CONST&&folded.v[2].immediate==5);
 assert(folded.v[4].op==MGX_SH_MUL);
 assert(mulib_glx_shadervm_run(&folded,inputs,out,scratch,32));
 assert(!memcmp(expected,out,sizeof out));
 /* Replacing an owning destination and in-place folding must not leak. */
 assert(mulib_glx_shaderjit_specialize(&shader,&folded));
 assert(mulib_glx_shaderjit_specialize(&folded,&folded));
 /* Inputs may overlap scratch; writes to registers must not change inputs. */
 memcpy(scratch,inputs,sizeof inputs);
 assert(mulib_glx_shadervm_run(&shader,scratch,out,scratch,32));
 assert(out[0]==20);
 inputs[7]=NAN;
 assert(!mulib_glx_shadervm_run(&shader,inputs,out,scratch,32));
 assert(out[0]==20);
 inputs[7]=FLT_MAX;
 assert(!mulib_glx_shadervm_run(&shader,inputs,out,scratch,32));
 assert(out[0]==20);
 mulib_glx_shaderir_free(&shader);
 mulib_glx_shaderir_free(&folded);
}

static void
test_cpu_renderer_fallback(void)
{
 mulib_glx_drawlist_t draws={0};
 mgx_surface image={0};
 mgx_drawcmd command={0};
 assert(!mulib_glx_gpu_draw(NULL,&draws,&image));
 assert(mgx_surface_init(&image,4,4));
 command.kind=MGX_RECT;
 command.bounds=command.clip=(mgx_rect){0,0,4,4};
 command.color=UINT32_C(0xff123456);
 assert(mulib_glx_drawlist_add(&draws,command));
 assert(mulib_glx_gpu_draw(NULL,&draws,&image));
 assert(image.pixels[0]==command.color);
 draws.v[0].kind=(mgx_draw_kind)99;
 assert(!mulib_glx_gpu_draw(NULL,&draws,&image));
 mulib_glx_drawlist_release(&draws);
 mgx_surface_free(&image);
}

int
main(void)
{
 test_validation();
 test_execution();
 test_cpu_renderer_fallback();
 return 0;
}
