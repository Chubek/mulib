#include "mulib-asof.h"
#include "mulib-backtest.h"
#include "mulib-corporate-actions.h"
#include "mulib-decisiontrace.h"
#include "mulib-fxrate.h"
#include "mulib-marketquote.h"
#include "mulib-orderbook.h"
#include "mulib-orderintent.h"
#include "mulib-portfolio.h"
#include "mulib-risk.h"
#include "mulib-ruleeval.h"
#include "mulib-sha.h"
#include "mulib-timeseries.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --wrap=malloc injects failure without changing the installed headers. */
void *__real_malloc(size_t size);
static int fail_malloc;
void *__wrap_malloc(size_t size){
    if(fail_malloc)return NULL;
    return __real_malloc(size);
}

static mulib_money_t eur(int64_t x){
    mulib_money_t m;
    assert(mulib_money_make(&m,x,"EUR",2));
    return m;
}
static uint32_t next_random(void){
    static uint32_t seed=UINT32_C(0x32046857);
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    return seed;
}

static void asof_oracle(void){
    enum { ROWS=64, KEYS=12 };
    const uint64_t keys[KEYS]={9,5,7,9,1,4,6,100,3,2,5,7};
    mulib_asof_row_t rows[ROWS];
    double v[KEYS];
    uint8_t found[KEYS];
    size_t t,i,j;
    for(t=0;t<256;++t){
        int64_t event=(int64_t)(next_random()%12),known=(int64_t)(next_random()%12);
        for(i=0;i<ROWS;++i){
            rows[i]=(mulib_asof_row_t){
                1+next_random()%10,next_random()%12,next_random()%12,next_random()%5,(double)i+1
            };
        }
        assert(mulib_asof_join(rows,ROWS,keys,KEYS,event,known,v,found,KEYS));
        for(i=0;i<KEYS;++i){
            unsigned rank=0;
            size_t want=SIZE_MAX,index=SIZE_MAX;
            mulib_asof_row_t chosen={0};
            int rc;
            for(j=0;j<ROWS;++j){
                unsigned r;
                if(rows[j].key!=keys[i]||rows[j].effective_at>event||rows[j].known_at>known)continue;
                /* A single integer independently orders the oracle's tiny timestamps. */
                r=(unsigned)rows[j].effective_at*1000+(unsigned)rows[j].known_at*10+
                  rows[j].revision+1;
                if(r>rank){rank=r;want=j;}
            }
            rc=mulib_asof_select(rows,ROWS,keys[i],event,known,&chosen,&index);
            assert(found[i]==(want!=SIZE_MAX)&&rc==found[i]);
            if(want==SIZE_MAX)assert(v[i]==0&&index==SIZE_MAX&&chosen.key==0);
            else assert(v[i]==rows[want].value&&chosen.value==v[i]&&index==want);
        }
    }
    {
        enum { BIG=4096 };
        static mulib_asof_row_t r[BIG];
        static uint64_t k[BIG];
        static double values[BIG];
        static uint8_t flags[BIG];
        for(i=0;i<BIG;++i){
            r[i]=(mulib_asof_row_t){i+1,1,2,0,(double)(i+1)};
            k[i]=BIG-i;
        }
        assert(mulib_asof_join(r,BIG,k,BIG,1,2,values,flags,BIG));
        for(i=0;i<BIG;++i)assert(flags[i]&&values[i]==(double)k[i]);
    }
}

static void asof_replay(void){
    uint64_t keys[]={1,2,3};
    mulib_asof_row_t r[]={
        {1,100,120,1,17500},{1,100,120,2,17400},{1,100,180,1,99},
        {1,110,110,0,18200},{2,100,120,0,17000},{2,100,140,0,9},
        {2,100,145,0,10},{3,100,100,0,-0.0},{3,100,100,0,0.0}
    };
    const uint8_t withdrawn[]={0,0,0,0,0,1,0,0,0};
    mulib_asof_snapshot_entry_t out[3];
    unsigned char saved[sizeof(out)];
    double values[]={3,4,5};
    uint8_t found[]={6,7,8};
    size_t index=123;
    mulib_asof_row_t selected={0};
    const size_t n=sizeof(r)/sizeof(r[0]);
    assert(mulib_asof_snapshot(r,n,withdrawn,keys,3,100,130,out,3));
    assert(out[0].status==MULIB_ASOF_PRESENT&&out[0].row.value==17400&&out[0].row.revision==2);
    assert(out[1].status==MULIB_ASOF_PRESENT&&out[1].row.value==17000);
    assert(out[2].status==MULIB_ASOF_PRESENT&&!signbit(out[2].row.value));
    assert(mulib_asof_snapshot(r,n,withdrawn,keys,3,100,140,out,3));
    assert(out[1].status==MULIB_ASOF_WITHDRAWN&&out[1].row.known_at==140&&out[1].row.value==0);
    {
        mulib_asof_row_t reversed[sizeof(r)/sizeof(r[0])];
        uint8_t reversed_flags[sizeof(withdrawn)];
        mulib_asof_snapshot_entry_t replay[3];
        size_t i;
        for(i=0;i<n;++i){
            reversed[i]=r[n-i-1];reversed_flags[i]=withdrawn[n-i-1];
        }
        assert(mulib_asof_snapshot(reversed,n,reversed_flags,keys,3,100,140,replay,3));
        for(i=0;i<3;++i){
            assert(replay[i].status==out[i].status&&replay[i].row.key==out[i].row.key&&
                   replay[i].row.effective_at==out[i].row.effective_at&&
                   replay[i].row.known_at==out[i].row.known_at&&
                   replay[i].row.revision==out[i].row.revision&&
                   replay[i].row.value==out[i].row.value);
        }
    }
    assert(mulib_asof_snapshot(r,n,withdrawn,keys,3,100,145,out,3));
    assert(out[1].status==MULIB_ASOF_PRESENT&&out[1].row.value==10);
    assert(mulib_asof_snapshot(r,n,withdrawn,keys,3,110,300,out,3));
    assert(out[0].row.value==18200); /* event-time rank takes precedence */
    assert(mulib_asof_snapshot(r,n,withdrawn,keys,3,100,180,out,3));
    assert(out[0].row.value==99);
    assert(mulib_asof_snapshot(NULL,0,NULL,keys,3,INT64_MAX,INT64_MAX,out,3));
    assert(out[1].status==MULIB_ASOF_MISSING&&out[1].row.key==2&&out[1].row.value==0);
    memcpy(saved,out,sizeof(out));

    fail_malloc=1;
    assert(!mulib_asof_snapshot(r,n,withdrawn,keys,3,100,150,out,3));
    assert(!mulib_asof_join(r,n,keys,3,100,150,values,found,3));
    fail_malloc=0;
    assert(!memcmp(saved,out,sizeof(out))&&values[0]==3&&values[1]==4&&values[2]==5);
    assert(found[0]==6&&found[1]==7&&found[2]==8);
    r[8].value=1; /* conflicting winning tie */
    assert(!mulib_asof_snapshot(r,n,withdrawn,keys,3,100,130,out,3));
    assert(!mulib_asof_snapshot(r,n,withdrawn,keys,3,100,130,out,2));
    assert(!memcmp(saved,out,sizeof(out)));
    assert(mulib_asof_select(r,n,3,100,130,&selected,&index)==1&&index==7);
    r[8].value=NAN;
    assert(mulib_asof_select(r,n,1,100,100,&selected,&index)==-1&&index==7&&selected.key==3);
    assert(!mulib_asof_join(r,n,keys,3,100,130,values,found,3));
    assert(values[0]==3&&found[0]==6);
    keys[2]=2; /* canonical snapshot keys cannot be duplicated */
    assert(!mulib_asof_snapshot(NULL,0,NULL,keys,3,100,130,out,3));
}

static void trace_wire(void){
    static const unsigned char expected[]={
        0x46,0x50,0x44,0x54,1,
        1,0,0,0,0,0,0,0, 3,0,0,0,0,0,0,0, 0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,
        120,0,0,0,0,0,0,0, 0xfb,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0x2c,1,0,0,0,0,0,0,
        3,0,0,0,0,0,0,0, 0x70,0x34,0x30
    };
    static const unsigned char want_sha[]={
        0x1e,0x62,0x87,0x60,0x70,0x6f,0x6c,0xa9,0xea,0x73,0x43,0x40,0x1e,0xad,0x23,0x0a,
        0xeb,0x00,0xc2,0xd0,0xda,0x61,0x81,0xc8,0x40,0xac,0x28,0xff,0x66,0xa2,0x01,0x7c
    };
    mulib_decisiontrace_event_t buf[1]={0};
    mulib_decisiontrace_event_t e={MULIB_DECISIONTRACE_RULE,UINT64_C(0x1122334455667788),120,-5,300,"p40"};
    mulib_decisiontrace_t t={0};
    unsigned char wire[sizeof(expected)+1]={0},digest[32],save[sizeof(buf)];
    size_t n=777;
    uint64_t fingerprint=0;
    assert(!mulib_decisiontrace_init(&t,buf,SIZE_MAX)&&t.capacity==0&&t.events==NULL);
    assert(mulib_decisiontrace_init(&t,buf,1));
    assert(mulib_decisiontrace_encode_v1(&t,NULL,0,&n)&&n==13);
    e.code[10]='X'; /* ignored tail is not canonical evidence */
    assert(mulib_decisiontrace_append(&t,&e)&&buf[0].code[10]==0);
    memcpy(save,buf,sizeof(buf));
    assert(!mulib_decisiontrace_append(&t,&e)&&t.count==1&&!memcmp(save,buf,sizeof(buf)));
    assert(mulib_decisiontrace_fingerprint64(&t,&fingerprint)&&
           fingerprint==UINT64_C(0xd050e691950828a1));
    assert(mulib_decisiontrace_encode_v1(&t,NULL,0,&n)&&n==sizeof(expected));
    assert(mulib_decisiontrace_encode_v1(&t,wire,n,&n)&&wire[sizeof(expected)]==0);
    assert(!memcmp(wire,expected,sizeof(expected)));
    assert(mulib_sha256(wire,n,digest)&&!memcmp(digest,want_sha,sizeof(digest)));
    assert(!mulib_decisiontrace_encode_v1(&t,wire,n-1,&n)&&!memcmp(wire,expected,n));
    buf[0].at=-1;
    assert(!mulib_decisiontrace_fingerprint64(&t,&fingerprint)&&
           fingerprint==UINT64_C(0xd050e691950828a1));
    assert(!mulib_decisiontrace_encode_v1(&t,wire,sizeof(wire),&n)&&n==sizeof(expected));
    assert(!memcmp(wire,expected,sizeof(expected)));
}

static void rule_oracle(void){
    mulib_ruleeval_policy_t p={0};
    mulib_ruleeval_result_t out={eur(-1),eur(-2),42};
    int lo,hi,tick,x;
    p.enable_floor=1;p.enable_ceiling=1;
    for(lo=0;lo<=12;++lo)for(hi=0;hi<=12;++hi)for(tick=1;tick<=8;++tick){
        p.floor=eur(lo);p.ceiling=eur(hi);p.tick_minor=tick;
        for(x=0;x<=14;++x){
            int y,want=-1,distance=100;
            int64_t before=out.recommended.minor;
            for(y=0;y<=hi;++y)if(y>=lo&&y%tick==0&&abs(y-x)<distance){
                want=y;distance=abs(y-x);
            }
            if(want<0)assert(!mulib_ruleeval_apply(&p,eur(x),eur(0),&out)&&
                             out.recommended.minor==before);
            else assert(mulib_ruleeval_apply(&p,eur(x),eur(0),&out)&&
                        out.recommended.minor==want&&out.requested.minor==x);
        }
    }
}

static void rule_policy(void){
    mulib_ruleeval_policy_t p={0};
    mulib_ruleeval_percentile_shortcut_t rule={5000,1000,MULIB_RULEEVAL_BELOW_THRESHOLD,
                                               MULIB_RULEEVAL_CLAMP_TO_FLOOR};
    mulib_ruleeval_percentile_quote_t p50={5000,{0}},p40={4000,{0}};
    mulib_ruleeval_result_t out={0};
    uint16_t lower=0;
    p.enable_shortcut=1;p.max_shortcut_difference_minor=500;
    p50.amount=eur(17500);p40.amount=eur(17000);
    assert(mulib_ruleeval_shortcut_percentile(&rule,&lower)&&lower==4000);
    assert(mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&
           out.recommended.minor==17500&&!out.fired_flags);
    assert(mulib_ruleeval_apply(&p,p50.amount,p40.amount,&out)&&out.recommended.minor==17000);
    rule.comparison=MULIB_RULEEVAL_AT_MOST_THRESHOLD;
    assert(mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&
           out.recommended.minor==17000&&out.fired_flags==MULIB_RULEEVAL_SHORTCUT);
    rule.comparison=MULIB_RULEEVAL_BELOW_THRESHOLD;p40.amount.minor=17001;
    p.enable_floor=1;p.floor=eur(17350);p.tick_minor=100;
    assert(mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&
           out.recommended.minor==17400&&out.fired_flags==
           (MULIB_RULEEVAL_SHORTCUT|MULIB_RULEEVAL_FLOOR|MULIB_RULEEVAL_TICK));
    rule.floor_precedence=MULIB_RULEEVAL_SKIP_BELOW_FLOOR;
    assert(mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&
           out.recommended.minor==17500&&!out.fired_flags);
    p.enable_previous=1;p.previous=eur(17000);p.max_change_bps=200;
    assert(!mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&out.recommended.minor==17500);
    p.enable_previous=0;rule.offset_bps=600;
    assert(mulib_ruleeval_shortcut_percentile(&rule,&lower)&&lower==4400);
    assert(!mulib_ruleeval_apply_percentiles(&p,&rule,p50,p40,&out)&&out.recommended.minor==17500);
    rule.offset_bps=5001;
    assert(!mulib_ruleeval_shortcut_percentile(&rule,&lower)&&lower==4400);
    p.enable_shortcut=0;p.floor=eur(INT64_MAX);
    p.tick_minor=2;
    assert(!mulib_ruleeval_apply(&p,eur(INT64_MAX),eur(0),&out));
    p.floor=eur(INT64_MAX-1);
    assert(mulib_ruleeval_apply(&p,eur(INT64_MAX),eur(0),&out)&&out.recommended.minor==INT64_MAX-1);
}

static void price_replay(void){
    mulib_asof_row_t market[]={
        {1,100,110,0,17500},{2,100,120,0,17001},{1,100,140,1,18001},{2,100,160,1,9999}
    };
    const uint64_t keys[]={1,2};
    mulib_ruleeval_policy_t p={0};
    const mulib_ruleeval_percentile_shortcut_t rule={5000,1000,MULIB_RULEEVAL_BELOW_THRESHOLD,
                                                     MULIB_RULEEVAL_CLAMP_TO_FLOOR};
    mulib_backtest_replay_row_t r[]={
        {{130,130,202,0,17200,17500},0,120,200},
        {{145,145,211,0,17900,17500},0,120,210}
    };
    mulib_backtest_summary_t out={0};
    uint64_t hash[2]={0};
    unsigned char saved[sizeof(out)];
    size_t i;
    p.enable_shortcut=1;p.max_shortcut_difference_minor=500;
    for(i=0;i<3;++i){
        size_t j=i%2;
        mulib_asof_snapshot_entry_t snapshot[2];
        mulib_ruleeval_percentile_quote_t high={5000,{0}},low={4000,{0}};
        mulib_ruleeval_result_t result;
        mulib_decisiontrace_t trace;
        mulib_decisiontrace_event_t ev={MULIB_DECISIONTRACE_RULE,0,0,0,0,"percentile"};
        mulib_decisiontrace_event_t buffer[1];
        uint64_t fingerprint;
        if(i==2){
            /* Change only data ingested after the first decision, then replay it. */
            market[2].value=20000;market[3].value=3000;
        }
        assert(mulib_asof_snapshot(market,4,NULL,keys,2,100,r[j].score.decision_at,snapshot,2));
        assert(snapshot[0].status==MULIB_ASOF_PRESENT&&snapshot[1].status==MULIB_ASOF_PRESENT);
        assert(snapshot[1].row.value==17001);
        r[j].inputs_known_at=snapshot[0].row.known_at>snapshot[1].row.known_at?
                             snapshot[0].row.known_at:snapshot[1].row.known_at;
        high.amount=eur((int64_t)snapshot[0].row.value);
        low.amount=eur((int64_t)snapshot[1].row.value);
        assert(mulib_ruleeval_apply_percentiles(&p,&rule,high,low,&result));
        assert(result.recommended.minor==(j==0?17001:18001));
        r[j].score.predicted=(double)result.recommended.minor;
        assert(mulib_decisiontrace_init(&trace,buffer,1));
        ev.at=r[j].score.decision_at;
        ev.before_minor=result.requested.minor;ev.after_minor=result.recommended.minor;
        assert(mulib_decisiontrace_append(&trace,&ev));
        assert(mulib_decisiontrace_fingerprint64(&trace,&fingerprint));
        if(i==2)assert(hash[0]==fingerprint);
        else hash[j]=fingerprint;
    }
    assert(hash[0]!=hash[1]);
    assert(mulib_backtest_score_replay(r,1,202,&out)&&out.mae==199&&out.baseline_mae==300);
    memcpy(saved,&out,sizeof(out));
    assert(!mulib_backtest_score_replay(r,2,202,&out)&&!memcmp(saved,&out,sizeof(out)));
    assert(mulib_backtest_score_replay(r+1,1,211,&out)&&out.mae==101&&out.baseline_mae==400);
    assert(mulib_backtest_score_replay(r,2,211,&out)&&out.mae==150&&out.bias==-49&&
           out.baseline_mae==350&&out.wape==300./35100.);
    memcpy(saved,&out,sizeof(out));
    r[1].inputs_known_at=146;
    assert(!mulib_backtest_score_replay(r,2,211,&out)&&!memcmp(saved,&out,sizeof(out)));
    r[1].inputs_known_at=145;r[1].baseline_known_at=146;
    assert(!mulib_backtest_score_replay(r,2,211,&out)&&!memcmp(saved,&out,sizeof(out)));
    r[1].baseline_known_at=145;r[1].actual_event_at=145;
    assert(!mulib_backtest_score_replay(r,2,211,&out)&&!memcmp(saved,&out,sizeof(out)));
    r[1].actual_event_at=212;
    assert(!mulib_backtest_score_replay(r,2,212,&out)&&!memcmp(saved,&out,sizeof(out)));
}

static void numeric_and_market(void){
    mulib_backtest_summary_t score={0};
    mulib_backtest_row_t r={1,1,1,DBL_MAX,0,0};
    mulib_timeseries_point_t points[]={{0,0,2},{1,3,100},{2,2,4},{3,4,8}},
                              latest={9,9,9};
    mulib_marketquote_t q={12,13,100,120,{0},{0},1,7};
    mulib_money_t spread=eur(-1),mid=eur(-2);
    double avg=-1;
    assert(mulib_backtest_score(&r,1,&score)&&score.rmse==DBL_MAX&&isnan(score.wape));
    r.predicted=DBL_MAX;r.actual=-DBL_MAX;
    assert(!mulib_backtest_score(&r,1,&score)&&score.mae==DBL_MAX);
    r.predicted=1;r.actual=DBL_MIN/16;
    assert(!mulib_backtest_score(&r,1,&score)&&isnan(score.wape)); /* overflowing WAPE */
    assert(!mulib_backtest_score(NULL,1,&score)&&score.observations==1);

    assert(mulib_timeseries_sma_asof(points,4,2,2,2,&avg)&&avg==3);
    assert(mulib_timeseries_latest_asof(points,4,1,2,&latest)==1&&latest.event_at==0);
    assert(mulib_timeseries_latest_asof(points+1,3,3,1,&latest)==0&&latest.value==2);
    assert(!mulib_timeseries_sma_asof(points,4,1,2,2,&avg)&&avg==3);
    points[3].event_at=2;
    assert(mulib_timeseries_latest_asof(points,4,1,2,&latest)==-1&&latest.value==2);
    assert(!mulib_timeseries_sma_asof(points,4,1,2,0,&avg)&&avg==3);

    q.bid=eur(INT64_MAX-3);q.ask=eur(INT64_MAX);
    assert(mulib_marketquote_spread_mid(&q,&spread,&mid)&&spread.minor==3&&mid.minor==INT64_MAX-2);
    assert(!mulib_marketquote_spread_mid(&q,&mid,&mid)&&mid.minor==INT64_MAX-2);
    q.bid=eur(100);q.ask=eur(105);
    assert(mulib_marketquote_usable_at(&q,120,20,5));
    assert(!mulib_marketquote_usable_at(&q,119,0,5)); /* event visible, receipt pending */
    assert(!mulib_marketquote_usable_at(&q,120,19,5));
    assert(!mulib_marketquote_usable_at(&q,120,20,10));
    q.ask.decimals=0;
    assert(!mulib_marketquote_spread_mid(&q,&spread,&mid)&&spread.minor==3&&mid.minor==INT64_MAX-2);
}

static void cash_boundaries(void){
    mulib_corporate_actions_split_t split={INT64_MAX,INT64_MAX};
    mulib_fxrate_t r={{'E','U','R',0},{'J','P','Y',0},2,0,1,2,100};
    mulib_money_t out=eur(-999);
    int64_t shares=987;
    assert(mulib_corporate_actions_adjust_shares(INT64_MAX,split,&shares)&&shares==INT64_MAX);
    split=(mulib_corporate_actions_split_t){3,2};
    assert(!mulib_corporate_actions_adjust_shares(1,split,&shares)&&shares==INT64_MAX);
    split=(mulib_corporate_actions_split_t){2,1};
    assert(!mulib_corporate_actions_adjust_shares(INT64_MAX,split,&shares)&&shares==INT64_MAX);
    assert(mulib_corporate_actions_adjust_price(eur(3),split,&out)&&out.minor==2);
    assert(!mulib_corporate_actions_adjust_price(eur(-3),split,&out)&&out.minor==2);
    assert(!mulib_corporate_actions_dividend(eur(INT64_MAX),2,&out)&&out.minor==2);
    assert(mulib_corporate_actions_dividend(eur(INT64_MAX),0,&out)&&out.minor==0);

    assert(!mulib_fxrate_convert_asof(eur(-3),&r,120,119,0,&out)&&out.minor==0);
    assert(!mulib_fxrate_convert_asof(eur(-3),&r,99,120,0,&out)&&out.minor==0);
    assert(mulib_fxrate_convert_asof(eur(-3),&r,120,120,20,&out)&&
           out.minor==-2&&out.decimals==0&&!memcmp(out.currency,"JPY",4));
    assert(!mulib_fxrate_convert_asof(eur(-3),&r,120,120,19,&out)&&out.minor==-2);
    r.numerator=3;r.denominator=1;
    assert(!mulib_fxrate_convert(eur(INT64_MIN),&r,120,0,&out)&&out.minor==-2);
    r.target_currency[3]='X';
    assert(!mulib_fxrate_convert(eur(1),&r,120,0,&out)&&out.minor==-2);
}

static void portfolio_intent(void){
    mulib_portfolio_position_t positions[2]={{23,1,{0},{0}},{12,-1,{0},{0}}};
    mulib_portfolio_summary_t sum={0};
    mulib_risk_result_t risk={0};
    mulib_risk_limits_t lim={eur(20),2,1000};
    mulib_orderintent_t i={123,7,MULIB_ORDERINTENT_BUY,2,{0},100,110};
    mulib_orderintent_policy_t policy={2,{0},5,1,0};
    mulib_money_t v=eur(-1);
    positions[0].average_cost=eur(20);positions[0].mark_price=eur(4);
    positions[1].average_cost=eur(20);positions[1].mark_price=eur(6);
    assert(mulib_portfolio_mark(positions,2,&sum)&&sum.unrealized_pnl.minor==-2);
    assert(mulib_risk_check(positions,2,&lim,&risk)&&risk.violations==4&&
           risk.worst_instrument_id==12);
    positions[1].mark_price=eur(4);
    assert(mulib_risk_check(positions,2,&lim,&risk)&&risk.worst_instrument_id==12);
    lim.max_single_name_bps=4999;
    assert(mulib_risk_check(positions,2,&lim,&risk)&&risk.violations==4);
    lim.max_single_name_bps=5000;
    assert(mulib_risk_check(positions,2,&lim,&risk)&&risk.allowed&&risk.worst_instrument_id==0);
    positions[1].instrument_id=23;
    assert(!mulib_risk_check(positions,2,&lim,&risk)&&risk.allowed);
    positions[1].instrument_id=12;positions[1].quantity=INT64_MIN;
    assert(!mulib_portfolio_mark(positions,2,&sum)&&sum.unrealized_pnl.minor==-2);
    positions[1].quantity=2;positions[1].average_cost=eur(INT64_MAX);
    assert(!mulib_portfolio_mark(positions,2,&sum)&&sum.gross_market_value.minor==10);
    assert(mulib_portfolio_gross(positions,2,&v)&&v.minor==12);
    assert(mulib_risk_check(positions,2,&lim,&risk)&&!risk.allowed&&risk.worst_instrument_id==12);
    positions[0].mark_price=eur(INT64_MAX);
    assert(!mulib_portfolio_gross(positions,2,&v)&&v.minor==12);
    assert(!mulib_risk_check(positions,2,&lim,&risk)&&risk.observed_gross_market_value.minor==12);

    policy.max_notional=eur(30);i.limit_price=eur(15);
    assert(!mulib_orderintent_validate(&i,&policy,99,&v)&&v.minor==12);
    assert(mulib_orderintent_validate(&i,&policy,100,&v)&&v.minor==30);
    assert(!mulib_orderintent_validate(&i,&policy,110,&v)&&v.minor==30);
    i.side=MULIB_ORDERINTENT_SELL;
    assert(!mulib_orderintent_validate(&i,&policy,105,&v)&&v.minor==30);
    i.side=MULIB_ORDERINTENT_BUY;i.limit_price.minor=14;
    assert(!mulib_orderintent_validate(&i,&policy,105,&v)&&v.minor==30);
    i.limit_price=eur(INT64_MAX);policy.min_tick_minor=1;
    assert(!mulib_orderintent_validate(&i,&policy,105,&v)&&v.minor==30);
    i.limit_price=eur(15);i.limit_price.decimals=3;
    assert(!mulib_orderintent_validate(&i,&policy,105,&v)&&v.minor==30);
}

static void book_recovery(void){
    mulib_orderbook_t b={0};
    mulib_orderbook_level_t storage[4]={0},best={0};
    mulib_orderbook_level_t snapshot[]={
        {MULIB_ORDERBOOK_BID,100,1},{MULIB_ORDERBOOK_BID,99,2},
        {MULIB_ORDERBOOK_ASK,101,3},{MULIB_ORDERBOOK_ASK,105,4}
    };
    unsigned char backup[sizeof(storage)];
    assert(mulib_orderbook_init(&b,storage,4));
    assert(mulib_orderbook_load_snapshot(&b,12,snapshot,4));
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_BID,&best)==1&&best.price_minor==100);
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_ASK,&best)==1&&best.price_minor==101);
    memcpy(backup,storage,sizeof(storage));
    assert(!mulib_orderbook_apply(&b,12,snapshot[0])); /* duplicate */
    assert(!mulib_orderbook_apply(&b,14,snapshot[0])); /* gap */
    assert(!mulib_orderbook_apply(&b,13,(mulib_orderbook_level_t){MULIB_ORDERBOOK_BID,98,1}));
    assert(!mulib_orderbook_apply(&b,13,(mulib_orderbook_level_t){MULIB_ORDERBOOK_BID,98,0}));
    snapshot[2].price_minor=98;
    assert(!mulib_orderbook_load_snapshot(&b,100,snapshot,4)); /* crossed */
    snapshot[2].price_minor=101;snapshot[1].price_minor=100;
    assert(!mulib_orderbook_load_snapshot(&b,100,snapshot,4)); /* duplicate level */
    assert(!memcmp(backup,storage,sizeof(storage))&&b.count==4&&b.last_sequence==12);
    assert(mulib_orderbook_apply(&b,13,(mulib_orderbook_level_t){MULIB_ORDERBOOK_ASK,105,0}));
    memcpy(backup,storage,sizeof(storage));
    assert(!mulib_orderbook_apply(&b,14,(mulib_orderbook_level_t){MULIB_ORDERBOOK_ASK,99,8}));
    assert(!memcmp(backup,storage,sizeof(storage))&&b.count==3&&b.last_sequence==13);
    fail_malloc=1;
    assert(mulib_orderbook_apply(&b,14,(mulib_orderbook_level_t){MULIB_ORDERBOOK_ASK,100,8}));
    fail_malloc=0;
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_ASK,&best)==1&&best.price_minor==100);
    assert(mulib_orderbook_load_snapshot(&b,UINT64_MAX,storage+2,2)); /* overlapping reset */
    assert(!mulib_orderbook_apply(&b,0,snapshot[0]));
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_BID,&best)==0&&best.quantity==8);
    storage[0].price_minor=0;
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_ASK,&best)==-1&&best.quantity==8);
    assert(mulib_orderbook_load_snapshot(&b,0,NULL,0));
    assert(mulib_orderbook_apply(&b,1,snapshot[0])&&b.count==1);
}

static void book_oracle(void){
    enum { PRICES=11, CAPACITY=5 };
    mulib_orderbook_t b;
    mulib_orderbook_level_t storage[CAPACITY]={0},best={0};
    uint64_t model[2][PRICES]={0},seq=0;
    size_t t;
    assert(mulib_orderbook_init(&b,storage,CAPACITY));
    for(t=0;t<4096;++t){
        const unsigned side=next_random()%2, price=next_random()%PRICES;
        const uint64_t qty=next_random()%8, request_seq=seq+1+(next_random()%8==0);
        uint64_t proposed[2][PRICES];
        unsigned count=0,i,hi=0,lo=PRICES;
        unsigned char backup[sizeof(storage)];
        int ok,expect;
        memcpy(proposed,model,sizeof(model));
        proposed[side][price]=qty;
        for(i=0;i<PRICES;++i){
            if(proposed[0][i]){hi=i;++count;}
            if(proposed[1][i]){if(lo==PRICES)lo=i;++count;}
        }
        expect=(request_seq==seq+1)&&(qty||model[side][price])&&count<=CAPACITY&&hi<=lo;
        memcpy(backup,storage,sizeof(storage));
        ok=mulib_orderbook_apply(&b,request_seq,(mulib_orderbook_level_t){
            side==0?MULIB_ORDERBOOK_BID:MULIB_ORDERBOOK_ASK,100+(int64_t)price,qty
        });
        assert(ok==expect);
        if(expect){
            ++seq;
            memcpy(model,proposed,sizeof(model));
            assert(b.count==count);
        }else assert(!memcmp(storage,backup,sizeof(storage)));
        assert(b.last_sequence==seq);
        for(i=0;i<2;++i){
            int j,want=-1;
            for(j=0;j<PRICES;++j)if(model[i][j]){
                want=j;
                if(i==1)break;
            }
            ok=mulib_orderbook_best(&b,i==0?MULIB_ORDERBOOK_BID:MULIB_ORDERBOOK_ASK,&best);
            assert(ok==(want>=0));
            if(want>=0)assert(best.price_minor==100+want&&best.quantity==model[i][want]);
        }
    }
}

int main(void){
    asof_oracle();asof_replay();trace_wire();rule_oracle();rule_policy();price_replay();
    numeric_and_market();cash_boundaries();portfolio_intent();book_recovery();book_oracle();
    puts("FinPRED replay, guardrail oracles, and boundary tests passed");
    return 0;
}
