/* Regression coverage for restricted image/color/nodegraph review candidates.
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined,float-cast-overflow
 *    -Iinclude tests/media-image-review-test.c -lm -o /tmp/opencode/image-review
 */
#include "mulib-img-blend.h"
#include "mulib-img-blur.h"
#include "mulib-img-convolve.h"
#include "mulib-img-core.h"
#include "mulib-img-denoise.h"
#include "mulib-img-edge.h"
#include "mulib-img-keying.h"
#include "mulib-img-matte.h"
#include "mulib-img-morphology.h"
#include "mulib-img-motionblur.h"
#include "mulib-img-opticalflow.h"
#include "mulib-img-pyramid.h"
#include "mulib-img-resample.h"
#include "mulib-img-sampler.h"
#include "mulib-img-tile.h"
#include "mulib-img-tracking.h"
#include "mulib-img-transform.h"
#include "mulib-img-warp.h"
#include "mulib-color-adaptation.h"
#include "mulib-color-cube.h"
#include "mulib-color-curves.h"
#include "mulib-color-gamut.h"
#include "mulib-color-grade.h"
#include "mulib-color-lut1d.h"
#include "mulib-color-lut3d.h"
#include "mulib-color-pipeline.h"
#include "mulib-color-primaries.h"
#include "mulib-color-qualifier.h"
#include "mulib-color-tonemap.h"
#include "mulib-color-transfer.h"
#include "mulib-color-vectorscope.h"
#include "mulib-color-waveform.h"
#include "mulib-color-wheels.h"
#include "mulib-nodegraph-cache.h"
#include "mulib-nodegraph-compile.h"
#include "mulib-nodegraph-core.h"
#include "mulib-nodegraph-dirty.h"
#include "mulib-nodegraph-eval.h"
#include "mulib-nodegraph-fusion.h"
#include "mulib-nodegraph-layout.h"
#include "mulib-nodegraph-region.h"
#include "mulib-nodegraph-serialize.h"
#include "mulib-nodegraph-temporal.h"
#include "mulib-nodegraph-types.h"
#include "mulib-nodegraph-validate.h"
#include <assert.h>

static uint32_t rng=UINT32_C(0x3bcdef12);
static uint32_t next_random(void){rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);return rng;}
static bool close_enough(double a,double b){return fabs(a-b)<1e-5;}
static mulib_img_rgba passthrough(void *ctx,mulib_img_rgba c){(void)ctx;return c;}

static void test_images(void){
    float pixels[40],saved[40],scratch[16],output[16],kernel[9]={0,0,0,0,1,0,0,0,0};
    for(size_t i=0;i<40;i++)pixels[i]=(float)(i%4==3?1:.25);
    memcpy(saved,pixels,sizeof pixels);
    mulib_img_view src={pixels,2,2,8,16},shift={pixels+4,2,2,8,16};
    mulib_img_view tmp={scratch,2,2,8,16},dst={output,2,2,8,16};
    double identity[9]={1,0,0,0,1,0,0,0,1};
    mulib_color_grade_cdl cdl={{1,1,1},{0,0,0},{1,1,1},1};
    mulib_color_pipeline_stage stage={passthrough,NULL};
    assert(mulib_img_valid(src)&&mulib_img_valid(shift));
    assert(!mulib_img_disjoint(src,shift));
    assert(!mulib_img_convolve(shift,src,kernel,9,3,3));
    assert(!mulib_img_convolve(dst,src,kernel,8,3,3));
    assert(!mulib_img_convolve(dst,src,output,9,3,3));
    assert(!mulib_img_blur_gaussian(dst,shift,src,1,1));
    assert(!mulib_img_edge_sobel(shift,src));
    assert(!mulib_img_morphology_alpha(shift,src,1,true));
    assert(!mulib_img_motionblur(shift,src,tmp,2));
    assert(!mulib_img_resample(shift,src,MULIB_IMG_BILINEAR));
    assert(!mulib_img_transform(shift,src,identity,MULIB_IMG_BILINEAR));
    assert(!mulib_img_warp(shift,src,tmp));
    assert(!mulib_img_denoise_bilateral(shift,src,1,1,1));
    assert(!mulib_img_matte(shift,src,0,1,false));
    assert(!mulib_img_keying(shift,src,0,1,0,0,1));
    assert(!mulib_img_blend(shift,src,tmp,MULIB_IMG_OVER));
    assert(!mulib_color_grade_apply(shift,src,cdl));
    assert(!mulib_color_pipeline_run(shift,src,&stage,1));
    mulib_img_view half={pixels+4,1,1,4,4};
    assert(!mulib_img_pyramid_down(half,src));
    assert(memcmp(saved,pixels,sizeof pixels)==0);
    assert(mulib_img_convolve(dst,src,kernel,9,3,3));
    assert(memcmp(output,pixels,sizeof output)==0);
    assert(mulib_img_blur_gaussian(src,tmp,src,1,1));
    for(size_t i=0;i<16;i++)assert(close_enough(pixels[i],saved[i]));
    assert(mulib_color_pipeline_run(src,src,NULL,0));
    assert(mulib_color_pipeline_run(src,src,&stage,1));
    assert(mulib_img_blend(src,src,src,MULIB_IMG_OVER));
    assert(mulib_color_grade_apply(src,src,cdl));
    mulib_img_view different_stride=src;
    different_stride.stride=12;different_stride.capacity=20;
    assert(mulib_img_valid(different_stride));
    assert(!mulib_img_matte(different_stride,src,0,1,false));
    mulib_img_view invalid=src;
    invalid.capacity=15;assert(!mulib_img_valid(invalid));
    invalid=src;invalid.stride=SIZE_MAX;assert(!mulib_img_valid(invalid));
    invalid=src;invalid.width=SIZE_MAX;assert(!mulib_img_valid(invalid));
    invalid=src;invalid.data=(float *)(uintptr_t)(UINTPTR_MAX-3);
    assert(!mulib_img_valid(invalid));
    assert(mulib_img_clamp_index(0,0)==0);
    mulib_img_rgba c=mulib_img_sample(src,NAN,0,MULIB_IMG_BILINEAR);
    assert(c.r==0&&c.a==0);
    c=mulib_img_sample(src,DBL_MAX,-DBL_MAX,MULIB_IMG_NEAREST);
    assert(close_enough(c.r,.25)&&c.a==1);
    for(int m=MULIB_IMG_NEAREST;m<=MULIB_IMG_LANCZOS3;m++){
        c=mulib_img_sample(src,.37,.81,(mulib_img_sampler_mode)m);
        assert(close_enough(c.r,.25)&&close_enough(c.a,1));
    }
    size_t cursor=0;
    mulib_img_rect rect;
    assert(mulib_img_tile_next(src,1,&cursor,&rect)&&rect.w==1&&rect.h==1);
    assert(mulib_img_tile_view(src,rect,&half)&&half.width==1);
    int dx=91,dy=92;
    double error=93;
    assert(mulib_img_opticalflow_block(src,src,0,0,1,2,&dx,&dy,&error));
    assert(dx==0&&dy==0&&error==0);
    assert(!mulib_img_opticalflow_block(src,src,0,0,1,2,&dx,&dx,&error));
    pixels[0]=NAN;dx=91;dy=92;error=93;
    assert(!mulib_img_opticalflow_block(src,src,0,0,1,2,&dx,&dy,&error));
    assert(dx==91&&dy==92&&error==93);
    mulib_img_tracking_point point={0,0,42,true};
    assert(!mulib_img_tracking_step(src,src,&point,1,99,1,0)&&point.valid&&point.error==42);
}

static FILE *input_file(const void *bytes,size_t n){
    FILE *f=tmpfile();
    assert(f);
    assert(fwrite(bytes,1,n,f)==n);
    rewind(f);
    return f;
}
static const char cube_identity[]=
    "# identity\r\nTITLE \"test\" # allowed comment\n"
    "LUT_3D_SIZE 2\nDOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 1\n"
    "0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1";

static void cube_failure(const char *input,size_t cap,size_t budget){
    float table[26];
    for(size_t i=0;i<26;i++)table[i]=17;
    mulib_color_lut3d result={NULL,7,8},saved=result;
    FILE *f=input_file(input,strlen(input));
    assert(!mulib_color_cube_read(f,table+1,cap,budget,&result));
    assert(result.rgb==saved.rgb&&result.edge==saved.edge&&result.count==saved.count);
    assert(table[0]==17&&table[25]==17);
    fclose(f);
}

static void test_color(void){
    float table[24];
    FILE *f=input_file(cube_identity,sizeof cube_identity-1);
    mulib_color_lut3d lut={0};
    assert(mulib_color_cube_read(f,table,24,16,&lut));
    fclose(f);
    assert(lut.edge==2&&lut.count==24);
    for(unsigned i=0;i<512;i++){
        mulib_img_rgba c={(next_random()%10001)/10000.0f,
                         (next_random()%10001)/10000.0f,
                         (next_random()%10001)/10000.0f,.37f};
        for(unsigned mode=0;mode<2;mode++){
            mulib_img_rgba d=mulib_color_lut3d_eval(lut,c,mode!=0);
            assert(close_enough(c.r,d.r)&&close_enough(c.g,d.g)&&
                   close_enough(c.b,d.b)&&c.a==d.a);
        }
    }
    cube_failure(cube_identity,23,16);
    cube_failure(cube_identity,24,1);
    cube_failure("LUT_3D_SIZE 429496729600000000000000000000000000000\n",24,16);
    cube_failure("LUT_3D_SIZE -2\n",24,16);
    cube_failure("LUT_3D_SIZE 2\n1e99999 0 0\n",24,16);
    cube_failure("LUT_3D_SIZE 2\n1e39 0 0\n",24,16);
    cube_failure("LUT_3D_SIZE 2\n0 0 0 junk\n",24,16);
    cube_failure("LUT_3D_SIZE 2\n0-1 0 0\n",24,16);
    cube_failure("LUT_3D_SIZE 2\nnan 0 0\n",24,16);
    cube_failure("DOMAIN_MIN 0 0 0\nDOMAIN_MIN 0 0 0\n",24,16);
    cube_failure("DOMAIN_MAX 1 1 1 junk\n",24,16);
    cube_failure("TITLE \"unterminated\n",24,16);
    char long_line[600];
    memset(long_line,'#',sizeof long_line);
    f=input_file(long_line,sizeof long_line);
    assert(!mulib_color_cube_read(f,table,24,16,&lut));
    fclose(f);
    char nul_line[]="LUT_3D_SIZE 2\n0 0\0 0\n";
    f=input_file(nul_line,sizeof nul_line-1);
    assert(!mulib_color_cube_read(f,table,24,16,&lut));
    fclose(f);
    for(unsigned i=0;i<256;i++){
        unsigned char bytes[96];
        size_t len=next_random()%sizeof bytes;
        for(size_t j=0;j<len;j++)bytes[j]=(unsigned char)next_random();
        float guarded[26];
        for(size_t j=0;j<26;j++)guarded[j]=17;
        f=input_file(bytes,len);
        mulib_color_lut3d out={NULL,123,456};
        bool ok=mulib_color_cube_read(f,guarded+1,24,8,&out);
        if(!ok)assert(!out.rgb&&out.edge==123&&out.count==456);
        assert(guarded[0]==17&&guarded[25]==17);
        fclose(f);
    }
    float hdr_lut[2]={-FLT_MAX,FLT_MAX};
    assert(mulib_color_lut1d_eval(hdr_lut,2,.5f)==0);
    assert(isnan(mulib_color_lut1d_eval(hdr_lut,SIZE_MAX,.5f)));
    assert(isnan(mulib_color_lut1d_eval(hdr_lut,2,NAN)));
    mulib_color_curve_point points[2]={{-FLT_MAX,-FLT_MAX},{FLT_MAX,FLT_MAX}};
    float value=42;
    assert(mulib_color_curves_eval(points,2,0,&value)&&value==0);
    points[1].x=NAN;
    assert(!mulib_color_curves_eval(points,2,0,&value)&&value==0);
    assert(mulib_color_tonemap_reinhard(INFINITY)==1);
    assert(isnan(mulib_color_tonemap_acesfit(NAN)));
    mulib_img_rgba bright=mulib_color_tonemap_pixel((mulib_img_rgba){0,1,FLT_MAX,.5f},FLT_MAX,false);
    assert(bright.r==0&&bright.g==1&&bright.b==1&&bright.a==.5f);
    for(unsigned i=0;i<100;i++){
        double x=i/99.0;
        assert(fabs(mulib_color_transfer_decode(MULIB_COLOR_TF_SRGB,
                    mulib_color_transfer_encode(MULIB_COLOR_TF_SRGB,x))-x)<1e-10);
    }
    float pixels[16]={0,0,0,1,1,1,1,1,NAN,0,0,1,FLT_MAX,-FLT_MAX,FLT_MAX,1};
    mulib_img_view src={pixels,2,2,8,16};
    uint32_t bins[18];
    for(size_t i=0;i<18;i++)bins[i]=99;
    assert(!mulib_color_vectorscope(src,bins+1,3,2));
    for(size_t i=0;i<18;i++)assert(bins[i]==99);
    assert(mulib_color_vectorscope(src,bins+1,4,2));
    uint32_t total=0;
    for(size_t i=1;i<5;i++)total+=bins[i];
    assert(total==3&&bins[0]==99&&bins[5]==99);
    assert(!mulib_color_waveform(src,bins+1,15,4,4,0));
    assert(mulib_color_waveform(src,bins+1,16,4,4,0));
    total=0;for(size_t i=1;i<17;i++)total+=bins[i];
    assert(total==3&&bins[0]==99&&bins[17]==99);
    assert(!mulib_color_vectorscope(src,(uint32_t *)(void *)pixels,16,2));
    assert(!mulib_color_waveform(src,(uint32_t *)(void *)pixels,16,2,2,0));
    mulib_color_mat3 bad={{NAN,0,0,0,1,0,0,0,1}},out={{42}};
    assert(!mulib_color_primaries_inverse(bad,&out)&&out.m[0]==42);
    double white[3]={.95047,1,1.08883};
    assert(mulib_color_adaptation_bradford(white,white,&out));
    for(size_t i=0;i<9;i++)assert(fabs(out.m[i]-(i%4==0?1:0))<1e-12);
}

static bool evaluate(void *ctx,size_t node,const size_t *inputs,size_t n,uintptr_t *out){
    size_t *calls=ctx;
    (*calls)++;
    if(node)assert(n==1&&inputs[0]==node-1);
    else assert(n==0);
    *out=node+1;
    return true;
}

static void test_graphs(void){
    uint32_t type=1;
    mulib_nodegraph_node nodes[3]={{1,1,1,0,NULL},{2,1,1,1,&type},{3,1,1,1,&type}};
    mulib_nodegraph_edge edges[2]={{0,1,0},{1,2,0}};
    mulib_nodegraph graph={nodes,3,edges,2};
    /* Outputs need no initialization, including in optimized -Werror builds. */
    size_t fresh_order[3],fresh_degree[3];
    assert(mulib_nodegraph_compile(graph,fresh_order,3,fresh_degree,3));
    assert(fresh_order[0]==0&&fresh_order[1]==1&&fresh_order[2]==2);
    assert(fresh_degree[0]==0&&fresh_degree[1]==0&&fresh_degree[2]==0);
    size_t order[4]={99,99,99,99},degree[4]={98,98,98,98};
    assert(!mulib_nodegraph_compile(graph,order,2,degree,3));
    assert(order[0]==99&&degree[0]==98);
    assert(!mulib_nodegraph_compile(graph,order,3,order+1,3));
    assert(order[0]==99&&order[3]==99);
    assert(mulib_nodegraph_compile(graph,order,3,degree,3));
    assert(order[0]==0&&order[1]==1&&order[2]==2&&order[3]==99&&degree[3]==98);
    mulib_nodegraph_position pos[3],pos_saved[3];
    size_t rows[3],rows_saved[3];
    memset(pos,0x25,sizeof pos);memcpy(pos_saved,pos,sizeof pos);
    memset(rows,0x26,sizeof rows);memcpy(rows_saved,rows,sizeof rows);
    mulib_nodegraph_region region[3]={{0,0,0,0},{0,0,0,0},{10,10,20,20}},region_saved[3];
    memcpy(region_saved,region,sizeof region);
    uint32_t halo[3]={0,2,3};
    uintptr_t values[3]={71,72,73};
    size_t scratch[1],calls=0;
    size_t invalid[3][3]={{0,0,2},{2,1,0},{0,1,3}};
    for(size_t i=0;i<3;i++){
        assert(!mulib_nodegraph_layout(graph,invalid[i],3,pos,3,rows,3));
        assert(memcmp(pos,pos_saved,sizeof pos)==0&&memcmp(rows,rows_saved,sizeof rows)==0);
        assert(!mulib_nodegraph_region_backprop(graph,invalid[i],3,region,3,halo,3));
        assert(memcmp(region,region_saved,sizeof region)==0);
        assert(!mulib_nodegraph_eval(graph,invalid[i],3,values,3,scratch,1,evaluate,&calls));
        assert(calls==0&&values[0]==71&&values[1]==72&&values[2]==73);
    }
    assert(!mulib_nodegraph_layout(graph,order,2,pos,3,rows,3));
    assert(!mulib_nodegraph_layout(graph,order,3,pos,2,rows,3));
    assert(!mulib_nodegraph_layout(graph,order,3,pos,3,order,3));
    assert(!mulib_nodegraph_region_backprop(graph,order,3,region,3,halo,2));
    assert(!mulib_nodegraph_region_backprop(graph,order,3,region,2,halo,3));
    assert(!mulib_nodegraph_eval(graph,order,3,values,2,scratch,1,evaluate,&calls));
    assert(!mulib_nodegraph_eval(graph,order,3,values,3,scratch,0,evaluate,&calls)&&calls==0);
    assert(!mulib_nodegraph_eval(graph,order,3,values,3,order,3,evaluate,&calls)&&calls==0);
    assert(mulib_nodegraph_eval(graph,order,3,values,3,scratch,1,evaluate,&calls)&&calls==3);
    assert(values[0]==1&&values[1]==2&&values[2]==3);
    assert(mulib_nodegraph_layout(graph,order,3,pos,3,rows,3));
    assert(pos[0].layer==0&&pos[1].layer==1&&pos[2].layer==2);
    assert(mulib_nodegraph_region_backprop(graph,order,3,region,3,halo,3));
    assert(region[0].x0==5&&region[0].x1==25&&region[1].x0==7);
    bool dirty[3]={true,false,false},pure[3]={true,true,true};
    assert(!mulib_nodegraph_dirty_propagate(graph,dirty,2)&&!dirty[1]);
    assert(mulib_nodegraph_dirty_propagate(graph,dirty,3)&&dirty[1]&&dirty[2]);
    assert(!mulib_nodegraph_fusion_candidate(graph,pure,2,0));
    assert(mulib_nodegraph_fusion_candidate(graph,pure,3,0));
    type=0;assert(!mulib_nodegraph_validate(graph));type=1;
    mulib_nodegraph empty={0};
    assert(mulib_nodegraph_compile(empty,NULL,0,NULL,0));
    assert(mulib_nodegraph_layout(empty,NULL,0,NULL,0,NULL,0));
    assert(mulib_nodegraph_region_backprop(empty,NULL,0,NULL,0,NULL,0));
    assert(mulib_nodegraph_eval(empty,NULL,0,NULL,0,NULL,0,evaluate,&calls));
    mulib_nodegraph_time_range time={INT64_MIN,INT64_MAX},expanded={12,13};
    assert(!mulib_nodegraph_temporal_expand(time,1,0,&expanded)&&expanded.first==12);
    mulib_nodegraph_region saturated=mulib_nodegraph_region_expand(
        (mulib_nodegraph_region){INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX},UINT32_MAX);
    assert(saturated.x0==INT32_MIN&&saturated.x1==INT32_MAX);
    mulib_nodegraph_cache_entry cache[2]={{0}},entry={1,2,3,4,true};
    assert(mulib_nodegraph_cache_insert(cache,2,entry));
    entry.value=5;
    assert(!mulib_nodegraph_cache_insert(cache,2,entry));
    uintptr_t cached=0;
    assert(mulib_nodegraph_cache_lookup(cache,2,1,2,3,&cached)&&cached==4);
}

/* Independent rank/closure oracle for randomized bounded graphs. */
static void test_graph_oracle(void){
    for(unsigned trial=0;trial<512;trial++){
        uint32_t types[6]={1,1,1,1,1,1};
        mulib_nodegraph_node nodes[6];
        mulib_nodegraph_edge edges[30];
        bool adjacency[6][6]={{false}};
        size_t n=1+next_random()%6,ne=0,permutation[6],rank[6],order[6],degree[6];
        for(size_t i=0;i<n;i++){
            nodes[i]=(mulib_nodegraph_node){i+1,1,1,(uint32_t)n,types};
            permutation[i]=i;
            for(size_t j=0;j<n;j++)if(i!=j&&next_random()%4==0){
                adjacency[i][j]=true;
                edges[ne++]=(mulib_nodegraph_edge){i,j,(uint32_t)i};
            }
        }
        for(size_t i=n;i>1;i--){
            size_t j=next_random()%i,t=permutation[i-1];
            permutation[i-1]=permutation[j];permutation[j]=t;
        }
        for(size_t i=0;i<n;i++)rank[permutation[i]]=i;
        bool valid_order=true;
        for(size_t i=0;i<ne;i++)if(rank[edges[i].from]>=rank[edges[i].to])valid_order=false;
        mulib_nodegraph graph={nodes,n,edges,ne};
        assert(mulib_nodegraph_order_valid(graph,permutation,n)==valid_order);
        for(size_t k=0;k<n;k++)for(size_t i=0;i<n;i++)for(size_t j=0;j<n;j++)
            adjacency[i][j]=adjacency[i][j]||(adjacency[i][k]&&adjacency[k][j]);
        bool acyclic=true;
        for(size_t i=0;i<n;i++)if(adjacency[i][i])acyclic=false;
        assert(mulib_nodegraph_compile(graph,order,n,degree,n)==acyclic);
        if(acyclic){
            for(size_t i=0;i<n;i++)rank[order[i]]=i;
            for(size_t i=0;i<ne;i++)assert(rank[edges[i].from]<rank[edges[i].to]);
        }
    }
}

int main(void){
    test_images();
    test_color();
    test_graphs();
    test_graph_oracle();
    puts("Image/color/nodegraph review regressions passed");
    return 0;
}
