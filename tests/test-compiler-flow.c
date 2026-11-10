#define MULIB_IMPL_CFG
#define MULIB_IMPL_DOMINANCE
#define MULIB_IMPL_POSTDOMINANCE
#define MULIB_IMPL_LOOPS
#define MULIB_IMPL_LIVENESS
#define MULIB_IMPL_DEFUSE
#define MULIB_IMPL_DCE
#define MULIB_IMPL_GVN
#define MULIB_IMPL_IR_VERIFY
#define MULIB_IMPL_IR_PRINT
#define MULIB_IMPL_RANGE_ANALYSIS
#include "mulib-postdominance.h"
#include "mulib-loops.h"
#include "mulib-liveness.h"
#include "mulib-dce.h"
#include "mulib-gvn.h"
#include "mulib-ir-print.h"
#include "mulib-range-analysis.h"
#include <assert.h>

/* Independent reachability oracle: removing a dominator disconnects its targets. */
static uint64_t reach_without(const mulib_cfg_t *g,size_t removed) {
    uint64_t seen=removed==0?0:1,old;
    do {old=seen;for(size_t i=0;i<g->count;i++)
        if(g->edges[i].to!=removed&&(seen&(UINT64_C(1)<<g->edges[i].from)))
            seen|=UINT64_C(1)<<g->edges[i].to;
    } while(old!=seen);
    return seen;
}
static void exhaustive_dominance(void) {
    /* All directed three-block graphs, including self loops and disconnected nodes. */
    for(unsigned mask=0;mask<512;mask++) {
        mulib_cfg_t g={0};g.blocks=3;
        for(size_t a=0;a<3;a++)for(size_t b=0;b<3;b++)
            if(mask&(1u<<(a*3+b)))assert(mulib_cfg_add(&g,a,b));
        mulib_dominance_t d;assert(mulib_dominance_compute(&g,0,&d));
        for(size_t a=0;a<3;a++)for(size_t b=0;b<3;b++) {
            int expected=!!(d.reachable&(UINT64_C(1)<<b))&&!(reach_without(&g,a)&(UINT64_C(1)<<b));
            assert(!!mulib_dominance_test(&d,a,b)==expected);
        }
        mulib_cfg_free(&g);
    }
}
int main(void) {
    mulib_cfg_t g={0};g.blocks=5;
    assert(mulib_cfg_add(&g,0,1)&&mulib_cfg_add(&g,1,2)&&mulib_cfg_add(&g,2,1));
    assert(mulib_cfg_add(&g,1,3)&&mulib_cfg_add(&g,4,2)); /* unreachable predecessor */
    assert(mulib_cfg_add(&g,1,2)&&g.count==5&&!mulib_cfg_add(&g,5,0));
    uint64_t reach=0;assert(mulib_cfg_reachable(&g,0,&reach)&&reach==15);
    size_t order[5]={99,99,99,99,99},count=99;
    assert(!mulib_cfg_postorder(&g,0,order,3,&count)&&order[0]==99&&count==99);
    assert(mulib_cfg_postorder(&g,0,order,4,&count)&&count==4&&order[0]==2&&order[3]==0);
    mulib_dominance_t d,pd;
    assert(mulib_dominance_compute(&g,0,&d)&&d.idom[2]==1&&d.idom[3]==1&&d.idom[4]==SIZE_MAX);
    assert(mulib_postdominance_compute(&g,3,&pd)&&mulib_dominance_test(&pd,1,0));
    assert(!mulib_postdominance_compute(&g,1,&pd));
    mulib_loops_t loops;assert(mulib_loops_find(&g,&d,&loops)&&loops.count==1&&loops.loops[0].members==6);
    uint64_t use[5]={0,0,1,2,0},def[5]={1,2,0,0,0};mulib_liveness_t live;
    assert(mulib_liveness_compute(&g,use,def,&live));
    for(size_t b=0;b<5;b++) {
        uint64_t out=0;for(size_t e=0;e<g.count;e++)if(g.edges[e].from==b)out|=live.in[g.edges[e].to];
        assert(live.out[b]==out&&live.in[b]==(use[b]|(out&~def[b])));
    }
    g.edges[4].from=64;
    assert(!mulib_cfg_validate(&g)&&!mulib_liveness_compute(&g,use,def,&live));
    assert(!mulib_loops_find(&g,&d,&loops));mulib_cfg_free(&g);
    mulib_defuse_inst_t code[]={{0,{0,0},0,0},{1,{0,0},1,0},{2,{0,0},0,0},{SIZE_MAX,{1,0},1,1}};
    mulib_defuse_info_t info[3];unsigned char marks[4];
    assert(mulib_defuse_analyze(code,4,3,info)&&info[0].use_count==1&&info[2].first_definition==2);
    assert(mulib_dce_mark(code,4,0,marks)&&marks[0]&&marks[1]&&!marks[2]&&marks[3]);
    memset(marks,9,sizeof marks);code[0].uses=3;
    assert(!mulib_dce_mark(code,4,0,marks)&&marks[3]==9);
    assert(!mulib_defuse_analyze(code,4,3,info));
    mulib_gvn_t gvn={0};size_t a,b;
    assert(mulib_gvn_intern(&gvn,1,8,3,1,&a)&&mulib_gvn_intern(&gvn,1,3,8,1,&b)&&a==b);
    assert(mulib_gvn_intern(&gvn,2,8,3,0,&a)&&mulib_gvn_intern(&gvn,2,3,8,0,&b)&&a!=b);
    mulib_gvn_free(&gvn);
    mulib_ir_instruction_t ir[]={{MULIB_IR_LITERAL,0,0,0,7},{MULIB_IR_COPY,1,0,0,0},{MULIB_IR_RETURN,0,1,0,0}};
    size_t bad;char text[256];
    assert(mulib_ir_verify(ir,3,2,&bad)&&bad==SIZE_MAX);
    assert(mulib_ir_print(ir,3,text,sizeof text)&&strstr(text,"1: copy r1"));
    assert(!mulib_ir_print(ir,3,text,2)&&text[0]==0);
    ir[1].a=1;assert(!mulib_ir_verify(ir,3,2,&bad)&&bad==1);
    ir[0].op=(mulib_ir_opcode_t)-1;assert(!mulib_ir_print(ir,3,text,sizeof text));
    mulib_range_t r=mulib_range_sub(mulib_range_const(INT64_MIN),mulib_range_const(INT64_MIN));
    assert(r.known&&r.lo==0&&r.hi==0);
    assert(!mulib_range_add(mulib_range_const(INT64_MAX),mulib_range_const(1)).known);
    r=mulib_range_mul((mulib_range_t){-3,2,1},(mulib_range_t){-4,5,1});
    assert(r.known&&r.lo==-15&&r.hi==12);
    assert(!mulib_range_mul(mulib_range_const(INT64_MIN),mulib_range_const(-1)).known);
    assert(!mulib_range_join((mulib_range_t){2,1,1},mulib_range_const(0)).known);
    for(int a=-5;a<=5;a++)for(int b=a;b<=5;b++)for(int c=-5;c<=5;c++)for(int e=c;e<=5;e++) {
        r=mulib_range_mul((mulib_range_t){a,b,1},(mulib_range_t){c,e,1});
        for(int x=a;x<=b;x++)for(int y=c;y<=e;y++)assert(r.known&&x*y>=r.lo&&x*y<=r.hi);
    }
    exhaustive_dominance();return 0;
}
