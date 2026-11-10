#define MULIB_IMPL_BRANCH_LAYOUT
#define MULIB_IMPL_CALLCONV
#define MULIB_IMPL_FRAME_LAYOUT
#define MULIB_IMPL_PHYSREG
#define MULIB_IMPL_LIVEINTERVALS
#define MULIB_IMPL_REGPRESSURE
#define MULIB_IMPL_REGALLOC_LINEAR
#define MULIB_IMPL_REGALLOC_GRAPH
#define MULIB_IMPL_REGALLOC
#include "mulib-branch-layout.h"
#include "mulib-callconv.h"
#include "mulib-frame-layout.h"
#include "mulib-regpressure.h"
#include "mulib-regalloc.h"
#include <assert.h>

int main(void) {
    mulib_physreg_t regs={UINT64_C(1)<<63,0,0,UINT64_C(1)<<63};unsigned index;
    assert(mulib_physreg_pick(&regs,0,&index)&&index==63);
    assert(!mulib_physreg_pick(&regs,UINT64_MAX,&index));
    regs.caller_saved=regs.callee_saved;assert(!mulib_physreg_validate(&regs));
    mulib_liveinterval_t ranges[]={{10,0,10},{11,1,2},{12,3,4},{13,4,4}};
    assert(mulib_liveinterval_overlap(ranges+2,ranges+3));
    assert(!mulib_liveinterval_overlap(ranges+1,ranges+2));
    size_t peak,spills;mulib_regalloc_location_t loc[4];
    assert(mulib_regpressure_peak(ranges,4,&peak)&&peak==3);
    assert(mulib_regalloc_linear(ranges,4,1,loc,&spills)&&spills==2);
    assert(loc[0].reg==-1&&loc[1].reg==0&&loc[2].reg==0&&loc[3].reg==-1);
    assert(loc[0].spill!=loc[3].spill);
    uint64_t graph[]={14,1,9,5};
    for(int algorithm=0;algorithm<2;algorithm++) {
        assert(mulib_regalloc_allocate((mulib_regalloc_algorithm_t)algorithm,ranges,4,graph,3,loc,&spills));
        for(size_t a=0;a<4;a++)for(size_t b=a+1;b<4;b++)
            if(mulib_liveinterval_overlap(ranges+a,ranges+b)&&loc[a].reg>=0&&loc[b].reg>=0)
                assert(loc[a].reg!=loc[b].reg);
    }
    assert(mulib_regalloc_allocate(MULIB_REGALLOC_GRAPH,NULL,0,NULL,0,NULL,&spills)&&!spills);
    int colors[4]={77,77,77,77};graph[0]=0;
    assert(!mulib_regalloc_graph(graph,4,3,colors,&spills)&&colors[0]==77);
    ranges[0].start=11;loc[0].reg=77;
    assert(!mulib_regalloc_linear(ranges,4,3,loc,&spills)&&loc[0].reg==77);
    assert(!mulib_regpressure_peak(ranges,4,&peak));
    mulib_callconv_location_t args[10];size_t size;
    assert(mulib_callconv_arguments(MULIB_CALLCONV_AMD64_SYSV,8,args,&size)&&size==16);
    assert(args[0].reg==7&&args[5].reg==9&&args[6].reg==-1&&args[7].stack_offset==8);
    assert(mulib_callconv_arguments(MULIB_CALLCONV_RV64_LP64,10,args,&size)&&size==16&&args[7].reg==17);
    assert(!mulib_callconv_arguments((mulib_callconv_abi_t)-1,0,NULL,&size));
    mulib_frame_layout_t frame={0};size_t id;
    assert(mulib_frame_layout_add(&frame,3,1,&id)&&id==0);
    assert(mulib_frame_layout_add(&frame,8,8,&id)&&frame.slots[id].offset==8);
    assert(mulib_frame_layout_finish(&frame,16,&size)&&size==16);
    assert(!mulib_frame_layout_add(&frame,1,3,&id)&&frame.used==16);
    assert(!mulib_frame_layout_add(&frame,SIZE_MAX,8,&id)&&frame.used==16);
    assert(!mulib_frame_layout_finish(&frame,0,&size));mulib_frame_layout_free(&frame);
    size_t fixed[]={0,127,0},offsets[3]={99,99,99},total=99;
    unsigned char wide[3]={9,9,9};
    mulib_branch_t branches[]={{2,2,5,0},{SIZE_MAX,0,0,0},{SIZE_MAX,0,0,0}};
    assert(mulib_branch_layout(3,fixed,branches,offsets,wide,&total)&&!wide[0]&&total==129);
    fixed[1]=128;
    assert(mulib_branch_layout(3,fixed,branches,offsets,wide,&total)&&wide[0]&&total==133);
    branches[0].target=SIZE_MAX;branches[2]=(mulib_branch_t){0,2,5,0};fixed[1]=126;
    assert(mulib_branch_layout(3,fixed,branches,offsets,wide,&total)&&!wide[2]);
    fixed[1]=127;
    assert(mulib_branch_layout(3,fixed,branches,offsets,wide,&total)&&wide[2]);
    fixed[1]=(size_t)INT32_MAX+1;offsets[0]=77;total=77;
    assert(!mulib_branch_layout(3,fixed,branches,offsets,wide,&total)&&offsets[0]==77&&total==77);
    fixed[1]=SIZE_MAX;
    assert(!mulib_branch_layout(3,fixed,branches,offsets,wide,&total));
    return 0;
}
