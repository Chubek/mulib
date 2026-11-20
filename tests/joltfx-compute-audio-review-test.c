/* Restricted new-kernel regression suite. No native GPU/audio device is used.
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
 *   -fsanitize=address,undefined -fno-omit-frame-pointer -g \
 *   tests/joltfx-compute-audio-review-test.c -lm -o /tmp/opencode/jfx-ca-test
 */
#include "mulib-audio.h" /* Canonical PCM declaration coexistence. */
#include "mulib-glx-shaderir.h"
#include "mulib-audio-channelmap.h"
#include "mulib-audio-device.h"
#include "mulib-audio-dynamics.h"
#include "mulib-audio-envelope.h"
#include "mulib-audio-eq.h"
#include "mulib-audio-loudness.h"
#include "mulib-audio-mixer.h"
#include "mulib-audio-resample.h"
#include "mulib-audio-reverb.h"
#include "mulib-audio-timestretch.h"
#include "mulib-compute-cache.h"
#include "mulib-compute-cpu.h"
#include "mulib-compute-d3d12.h"
#include "mulib-compute-device.h"
#include "mulib-compute-image.h"
#include "mulib-compute-memory.h"
#include "mulib-compute-metal.h"
#include "mulib-compute-spirv.h"
#include "mulib-compute-sync.h"
#include "mulib-compute-vulkan.h"
#include "mulib-compute-webgpu.h"
#include "mulib-kernel-fusion.h"
#include "mulib-kernel-legalize.h"
#include "mulib-kernel-memory.h"
#include "mulib-kernel-types.h"
#include "mulib-kernel-uniformity.h"
#include "mulib-kernel-vectorize.h"
#include "mulib-kernel-wgsl.h"
#include <assert.h>

typedef struct {size_t calls,closed;uint64_t fence;mulib_media_status status;size_t written;} test_provider;
static mulib_media_status dispatch(void *ctx,uint64_t pipeline,mulib_compute_grid g,const mulib_compute_handle *h,size_t n,uint64_t *f){
 test_provider *s=ctx;(void)h;assert(pipeline==1&&g.x==1&&n==1);
 s->calls++;*f=s->fence;return s->status;
}
static mulib_media_status wait_fence(void *ctx,uint64_t f,uint64_t timeout){(void)ctx;(void)timeout;assert(f==7);return MULIB_MEDIA_OK;}
static void close_provider(void *ctx){((test_provider *)ctx)->closed++;}
static bool cpu_work(void *ctx,uint32_t x,uint32_t y,uint32_t z){(void)x;(void)y;(void)z;(*(unsigned *)ctx)++;return true;}
static bool cpu_cancel(void *ctx,uint32_t x,uint32_t y,uint32_t z){(void)x;(void)y;(void)z;(*(unsigned *)ctx)++;return false;}
static mulib_media_status audio_control(void *ctx){((test_provider *)ctx)->calls++;return MULIB_MEDIA_OK;}
static mulib_media_status audio_write(void *ctx,const float *p,size_t n,size_t *done){
 test_provider *s=ctx;(void)p;(void)n;s->calls++;*done=s->written;return s->status;
}

static void test_compute(void){
 uint32_t words[12]={0x07230203u,0x00010600u,0,1,0,0,0};
 assert(mulib_compute_spirv_validate(words,5));
 size_t used=5;
 words[5]=17;words[6]=23;
 assert(mulib_compute_spirv_emit(words,12,&used,42,words+5,2));
 assert(used==8&&words[5]==((3u<<16)|42)&&words[6]==17&&words[7]==23);
 assert(mulib_compute_spirv_validate(words,used));
 size_t save=used;
 assert(!mulib_compute_spirv_emit(words,8,&used,1,words,1)&&used==save);
 words[5]=0;assert(!mulib_compute_spirv_validate(words,8));
 words[5]=4u<<16;assert(!mulib_compute_spirv_validate(words,8));
 words[1]=0x00010700u;assert(!mulib_compute_spirv_validate(words,5));
 assert(!mulib_compute_spirv_validate(words,SIZE_MAX));
 unsigned calls=0;
 assert(mulib_compute_cpu_dispatch((mulib_compute_grid){2,3,4},cpu_work,&calls,24)==MULIB_MEDIA_OK&&calls==24);
 assert(mulib_compute_cpu_dispatch((mulib_compute_grid){UINT32_MAX,UINT32_MAX,UINT32_MAX},cpu_work,&calls,UINT64_MAX)==MULIB_MEDIA_RANGE&&calls==24);
 assert(mulib_compute_cpu_dispatch((mulib_compute_grid){1,1,1},cpu_cancel,&calls,1)==MULIB_MEDIA_CANCELLED&&calls==25);
 mulib_compute_memory_arena arena={32,1};size_t off=99;
 assert(mulib_compute_memory_alloc(&arena,8,8,&off)&&off==8&&arena.cursor==16);
 assert(!mulib_compute_memory_alloc(&arena,SIZE_MAX,8,&off)&&arena.cursor==16);
 arena.cursor=33;assert(!mulib_compute_memory_reset(&arena,0));
 mulib_compute_image image={0};
 assert(!mulib_compute_image_alloc(&image,SIZE_MAX,2,4,8)&&!image.buffer.data);
 assert(mulib_compute_image_alloc(&image,3,2,4,16)&&image.row_bytes==16&&image.buffer.size==32);
 unsigned char bytes[4]={1,2,3,4},out[4]={0};
 assert(mulib_compute_buffer_upload(image.buffer,28,bytes,4));
 assert(!mulib_compute_buffer_upload(image.buffer,29,bytes,4));
 assert(mulib_compute_buffer_download(image.buffer,28,out,4)&&!memcmp(bytes,out,4));
 mulib_compute_image_free(&image);mulib_compute_image_free(&image);
 mulib_compute_cache_entry cache[1]={{0}};
 assert(mulib_compute_cache_add(cache,1,(mulib_compute_cache_entry){1,2,3,4,false}));
 assert(mulib_compute_cache_add(cache,1,(mulib_compute_cache_entry){1,2,3,5,false}));
 uint64_t handle=0;assert(mulib_compute_cache_find(cache,1,1,2,3,&handle)&&handle==5);
 mulib_compute_device devices[]={{(mulib_compute_backend)99,"invalid",100,999,true},{MULIB_COMPUTE_CPU,"CPU",100,1,true}};
 assert(mulib_compute_device_select(devices,2,MULIB_COMPUTE_VULKAN,0,true)==1);
 assert(mulib_compute_device_select(devices,2,(mulib_compute_backend)99,0,true)==SIZE_MAX);
 test_provider s={0,0,7,MULIB_MEDIA_OK,0};
 mulib_compute_context provider={MULIB_COMPUTE_VULKAN,&s,{8,8,8},1024,dispatch,wait_fence,close_provider},context={0};
 assert(mulib_compute_vulkan_attach(&context,&provider)&&!provider.ctx);
 assert(!mulib_compute_vulkan_attach(&context,&context));
 size_t minimum=4;mulib_compute_pipeline p={1,{1,1,1},1,&minimum};
 mulib_compute_handle h={1,4};uint64_t fence=55;
 assert(mulib_compute_vulkan_submit(&context,p,(mulib_compute_grid){1,1,1},&h,1,&fence)==MULIB_MEDIA_OK&&fence==7&&s.calls==1);
 s.status=MULIB_MEDIA_IO;s.fence=999;fence=55;
 assert(mulib_compute_dispatch(&context,p,(mulib_compute_grid){1,1,1},&h,1,&fence)==MULIB_MEDIA_IO&&fence==55);
 s.status=MULIB_MEDIA_OK;s.fence=0;
 assert(mulib_compute_dispatch(&context,p,(mulib_compute_grid){1,1,1},&h,1,&fence)==MULIB_MEDIA_IO&&fence==55);
 h.bytes=1025;
 assert(mulib_compute_dispatch(&context,p,(mulib_compute_grid){1,1,1},&h,1,&fence)==MULIB_MEDIA_RANGE&&s.calls==3);
 assert(mulib_compute_sync_wait(&context,7,0)==MULIB_MEDIA_OK);
 assert(mulib_compute_sync_wait(&context,0,0)==MULIB_MEDIA_INVALID);
 mulib_compute_context_close(&context);mulib_compute_context_close(&context);assert(s.closed==1);
 mulib_compute_sync_timeline timeline={UINT64_MAX,0};
 assert(!mulib_compute_sync_submit(&timeline,&fence));
}

static void test_ir(void){
 mulib_kernel_inst code[]={
  {MULIB_KERNEL_PARAM,MULIB_KERNEL_F32,0,0,0,0},
  {MULIB_KERNEL_CONST,MULIB_KERNEL_F32,0,0,0,2},
  {MULIB_KERNEL_MUL,MULIB_KERNEL_F32,0,1,0,0}
 };
 mulib_kernel_ir ir={code,3,2,1};float param=3,scratch[8],value=99;
 assert(mulib_kernel_eval(ir,&param,1,scratch,8,&value)&&value==6);
 assert(!mulib_kernel_eval(ir,&param,0,scratch,8,&value)&&value==6);
 assert(!mulib_kernel_eval(ir,&param,1,scratch,2,&value));
 scratch[0]=3;assert(!mulib_kernel_eval(ir,scratch,1,scratch,8,&value)&&scratch[0]==3);
 mulib_kernel_inst fused[6];uint32_t map[3];mulib_kernel_ir result={0};
 assert(!mulib_kernel_fusion(ir,ir,fused,6,map,2,&result)&&!result.code);
 assert(!mulib_kernel_fusion(ir,ir,code,6,map,3,&result));
 assert(mulib_kernel_fusion(ir,ir,fused,6,map,3,&result));
 assert(mulib_kernel_eval(result,&param,1,scratch,8,&value)&&value==12);
 for(unsigned i=0;i<100;i++){
  float x=(float)i/7-3;
  assert(mulib_kernel_eval(result,&x,1,scratch,8,&value));
  assert(fabsf(value-4*x)<1e-5f);
 }
 bool uniform_param=true,uniform[3];
 assert(!mulib_kernel_uniformity(ir,&uniform_param,0,uniform,3));
 assert(mulib_kernel_uniformity(ir,&uniform_param,1,uniform,3)&&uniform[2]);
 assert(!mulib_kernel_uniformity(ir,uniform,1,uniform,3));
 float lanes[]={1,2,3,4},outs[4]={99,99,99,99};
 assert(!mulib_kernel_vectorize_eval(ir,lanes,3,4,scratch,8,outs,4)&&outs[0]==99);
 assert(!mulib_kernel_vectorize_eval(ir,lanes,4,4,scratch,8,outs,3));
 assert(mulib_kernel_vectorize_eval(ir,lanes,4,4,scratch,8,outs,4)&&outs[3]==8);
 assert(!mulib_kernel_vectorize_eval(ir,lanes,4,4,scratch,8,lanes,4));
 mulib_kernel_inst typed[]={
  {MULIB_KERNEL_CONST,MULIB_KERNEL_F32,0,0,0,1},
  {MULIB_KERNEL_CONST,MULIB_KERNEL_F32,0,0,0,2},
  {MULIB_KERNEL_LT,MULIB_KERNEL_BOOL,0,1,0,0},
  {MULIB_KERNEL_SELECT,MULIB_KERNEL_F32,2,1,0,0},
  {MULIB_KERNEL_SELECT,MULIB_KERNEL_BOOL,2,2,2,0}
 };
 mulib_kernel_ir ti={typed,5,3,0};
 assert(mulib_kernel_eval(ti,NULL,0,scratch,8,&value)&&value==2);
 ti.result=4;assert(mulib_kernel_eval(ti,NULL,0,scratch,8,&value)&&value==1);
 FILE *f=tmpfile();assert(f);
 assert(mulib_kernel_wgsl_emit(f,ti)&&mulib_kernel_glsl_emit(f,ti));
 rewind(f);char text[2048];size_t n=fread(text,1,sizeof text-1,f);text[n]=0;
 assert(strstr(text,"-> bool")&&strstr(text,"select(v2,v2,v2)")&&strstr(text,"bool mulib_kernel"));
 fclose(f);
 assert(mulib_kernel_legalize_fold(typed,ti)&&typed[4].op==MULIB_KERNEL_CONST&&typed[4].literal==1);
 typed[0].type=MULIB_KERNEL_I32;assert(!mulib_kernel_validate(ti));
 typed[0].type=MULIB_KERNEL_F32;typed[0].literal=NAN;assert(!mulib_kernel_validate(ti));
 mulib_kernel_inst constant={MULIB_KERNEL_CONST,MULIB_KERNEL_F32,0,0,0,1};
 mulib_kernel_ir oversized={&constant,1,0,UINT32_MAX};
 f=tmpfile();assert(f);
 assert(!mulib_kernel_glsl_emit(f,oversized)&&!mulib_kernel_wgsl_emit(f,oversized)&&ftell(f)==0);
 assert(!mulib_kernel_glsl_expr(f,(mulib_kernel_inst){(mulib_kernel_op)99,MULIB_KERNEL_F32,0,0,0,0},false));
 fclose(f);
 code[2].op=MULIB_KERNEL_MIN;param=NAN;
 assert(mulib_kernel_eval(ir,&param,1,scratch,8,&value)&&value==2);
 code[2].op=MULIB_KERNEL_DIV;param=INFINITY;
 assert(mulib_kernel_eval(ir,&param,1,scratch,8,&value)&&isinf(value));
 mulib_kernel_memory_region region={1,SIZE_MAX-2,4};
 assert(!mulib_kernel_memory_valid(region,SIZE_MAX));
 assert(mulib_kernel_memory_alias((mulib_kernel_memory_region){1,5,3},(mulib_kernel_memory_region){1,7,1}));
}

static void test_audio(void){
 float a[16]={1,2,3,4,5,6,7,8},copy[16],out[16]={0},matrix[]={1,0,0,1};
 memcpy(copy,a,sizeof a);
 assert(!mulib_audio_channelmap(a+1,2,a,2,2,matrix)&&!memcmp(copy,a,sizeof a));
 assert(!mulib_audio_channelmap(out,2,a,2,SIZE_MAX/2,matrix));
 assert(mulib_audio_channelmap(out,2,a,2,4,matrix)&&!memcmp(out,a,8*sizeof(float)));
 assert(!mulib_audio_resample_linear(a+1,3,a,4,1,1,0)&&!memcmp(copy,a,sizeof a));
 assert(!mulib_audio_resample_linear(out,1,a,SIZE_MAX,1,1,0));
 assert(mulib_audio_resample_linear(out,4,a,4,1,.5,0)&&out[0]==1&&out[1]==1.5f&&out[3]==2.5f);
 assert(mulib_audio_resample_linear(out,4,a,4,1,DBL_MAX,DBL_MAX)&&out[0]==4&&out[3]==4);
 assert(!mulib_audio_resample_linear(out,4,a,4,1,NAN,0));
 float hdr[]={FLT_MAX,FLT_MAX};
 assert(mulib_audio_resample_linear(out,2,hdr,2,1,.5,0)&&out[1]==FLT_MAX);
 assert(mulib_audio_envelope(hdr,2,1,FLT_MAX,FLT_MAX,true)&&isinf(hdr[0]));
 float gains[]={1,1};
 assert(mulib_audio_envelope(gains,2,1,FLT_MAX,-FLT_MAX,false)&&gains[0]==FLT_MAX&&gains[1]==-FLT_MAX);
 assert(!mulib_audio_envelope(gains,2,1,-1,1,true));
 assert(!mulib_audio_envelope(gains,SIZE_MAX,2,1,1,false));
 mulib_audio_mixer_track tracks[]={{a,4,1,1,0,false}};
 assert(!mulib_audio_mixer(a+1,4,tracks,1)&&!memcmp(copy,a,sizeof a));
 assert(mulib_audio_mixer(out,4,tracks,1)&&fabsf(out[0]-.70710678f)<1e-6f&&out[0]==out[1]);
 tracks[0].frames=SIZE_MAX;assert(!mulib_audio_mixer(out,4,tracks,1));tracks[0].frames=4;
 a[2]=NAN;out[0]=99;assert(!mulib_audio_mixer(out,4,tracks,1)&&out[0]==99);a[2]=3;
 float taps[]={1,2};
 assert(!mulib_audio_filter_fir(a+1,a,4,taps,2)&&!memcmp(copy,a,sizeof a));
 assert(mulib_audio_filter_fir(out,a,4,taps,2)&&out[0]==1&&out[1]==4&&out[2]==7);
 mulib_audio_filter_biquad filter={1,2,3,4,5,6,7},old=filter;
 assert(!mulib_audio_filter_lowpass(&filter,48000,1000,DBL_TRUE_MIN)&&!memcmp(&filter,&old,sizeof filter));
 assert(mulib_audio_filter_lowpass(&filter,DBL_MAX,DBL_MAX/8,.7)&&mulib_audio_filter_valid(&filter));
 old=filter;assert(!mulib_audio_eq_peak(&filter,48000,1000,DBL_TRUE_MIN,6)&&!memcmp(&filter,&old,sizeof filter));
 assert(mulib_audio_eq_peak(&filter,DBL_MAX,DBL_MAX/8,.7,6)&&mulib_audio_filter_valid(&filter));
 assert(mulib_audio_eq_process(a,4,&filter,1));
 filter.a1=NAN;assert(!mulib_audio_eq_process(a,4,&filter,1));
 mulib_audio_dynamics dyn={0};
 assert(mulib_audio_dynamics_init(&dyn,.5f,4,48000,.01,.1));
 assert(isfinite(mulib_audio_dynamics_tick(&dyn,FLT_MAX)));
 assert(!mulib_audio_dynamics_init(&dyn,.5f,4,DBL_MAX,DBL_MAX,1));
 mulib_audio_loudness meter={0};
 float signal[]={1,-1,1,-1};assert(mulib_audio_loudness_update(&meter,signal,4));
 assert(mulib_audio_loudness_rms_db(meter)==0&&mulib_audio_loudness_peak_db(meter)==0);
 mulib_audio_loudness saved=meter;signal[1]=NAN;
 assert(!mulib_audio_loudness_update(&meter,signal,4)&&!memcmp(&meter,&saved,sizeof meter));
 meter.sum_squares=-1;assert(!mulib_audio_loudness_update(&meter,NULL,0));
 float ring1[2],ring2[3];mulib_audio_delay comb[2];
 assert(mulib_audio_delay_init(comb,ring1,2,.5f,1));
 assert(mulib_audio_delay_init(comb+1,ring2,3,.5f,1));
 float impulse[]={1,0,0,0};
 assert(mulib_audio_reverb_process(impulse,4,comb,2,1)&&impulse[0]==0&&impulse[2]==.5f&&impulse[3]==.5f);
 comb[1].buffer=ring1;
 assert(!mulib_audio_reverb_process(impulse,4,comb,2,1));comb[1].buffer=ring2;
 comb[0].feedback=NAN;assert(!mulib_audio_reverb_process(impulse,4,comb,2,1));
 float norm[16],ones[8]={1,1,1,1,1,1,1,1};size_t written=99;
 assert(mulib_audio_timestretch_ola(out,norm,16,ones,8,4,2,3,&written)&&written==10);
 for(size_t i=0;i<written;i++)assert(fabsf(out[i]-1)<1e-6f);
 assert(!mulib_audio_timestretch_ola(out,norm,4,ones,8,4,2,3,&written)&&written==10);
 assert(!mulib_audio_timestretch_ola(out,out+1,12,ones,8,4,2,3,&written));
 for(size_t i=0;i<8;i++)ones[i]=FLT_MAX;
 assert(mulib_audio_timestretch_ola(out,norm,16,ones,8,4,2,1,&written));
 for(size_t i=0;i<written;i++)assert(out[i]==FLT_MAX);
 test_provider state={0,0,0,MULIB_MEDIA_OK,2};
 mulib_audio_device dev={&state,2,48000,audio_write,audio_control,audio_control,close_provider,false};
 assert(mulib_audio_device_start(&dev)==MULIB_MEDIA_OK);
 size_t done=99;
 assert(mulib_audio_device_write(&dev,copy,4,&done)==MULIB_MEDIA_OK&&done==2);
 assert(mulib_audio_device_write(&dev,copy,SIZE_MAX,&done)==MULIB_MEDIA_INVALID&&done==2);
 state.written=5;
 assert(mulib_audio_device_write(&dev,copy,4,&done)==MULIB_MEDIA_IO&&done==0);
 assert(mulib_audio_device_stop(&dev)==MULIB_MEDIA_OK);
 mulib_audio_device_close(&dev);mulib_audio_device_close(&dev);assert(state.closed==1);
 dev=(mulib_audio_device){&state,2,48000,audio_write,audio_control,NULL,close_provider,false};
 assert(mulib_audio_device_start(&dev)==MULIB_MEDIA_INVALID);
}

/* Deterministic framing/type fuzz smoke, bounded buffers and work. */
static void test_mutations(void){
 uint32_t seed=1234567;
 for(unsigned trial=0;trial<10000;trial++){
  uint32_t words[12];
  for(size_t i=0;i<12;i++){seed=seed*1664525u+1013904223u;words[i]=seed;}
  words[0]=0x07230203u;words[1]=0x00010600u;words[3]=1;words[4]=0;
  (void)mulib_compute_spirv_validate(words,12);
  mulib_kernel_inst code[4];
  for(size_t i=0;i<4;i++){
   seed=seed*1664525u+1013904223u;
   code[i]=(mulib_kernel_inst){(mulib_kernel_op)(seed%14),(mulib_kernel_type)(seed%6),seed%5,(seed>>8)%5,(seed>>16)%5,(float)(seed%3)};
  }
  mulib_kernel_ir ir={code,4,seed%5,1};float param=1,scratch[4],out;
  if(mulib_kernel_validate(ir))assert(mulib_kernel_eval(ir,&param,1,scratch,4,&out));
 }
}

int main(void){
 test_compute();test_ir();test_audio();test_mutations();
 puts("JoltFX restricted compute/kernel/audio review tests passed");
 return 0;
}
