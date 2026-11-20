/* Declaration-only C11 client of the common implementation translation unit. */
#include "mulib-contract.h"
#include "mulib-ratequote.h"
#include "mulib-staymatch.h"
#include "mulib-compset.h"
#include "mulib-pricecurve.h"
#include "mulib-asof.h"
#include "mulib-decisiontrace.h"
#include "mulib-ruleeval.h"
#include "mulib-backtest.h"
#include "mulib-marketquote.h"
#include "mulib-timeseries.h"
#include "mulib-corporate-actions.h"
#include "mulib-fxrate.h"
#include "mulib-portfolio.h"
#include "mulib-risk.h"
#include "mulib-orderbook.h"
#include "mulib-orderintent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static mulib_money_t eur(int64_t minor){mulib_money_t a;assert(mulib_money_make(&a,minor,"EUR",2));return a;}
static mulib_money_t jpy(int64_t minor){mulib_money_t a;assert(mulib_money_make(&a,minor,"JPY",0));return a;}
static void contract_test(void){
    const char payload[]={0,1,0,2};
    mulib_contract_t c={"price-1","hospitality.offer","hospitality.offer.v1","dragnet",1,0,100,100,200,0,payload,sizeof(payload)};
    uint64_t h=0,b=0;
    assert(mulib_contract_valid(&c,sizeof(payload)));
    assert(mulib_contract_usable_at(&c,100,sizeof(payload)));
    assert(!mulib_contract_usable_at(&c,200,sizeof(payload)));
    assert(mulib_contract_fingerprint64(&c,sizeof(payload),&h));
    assert(mulib_contract_fingerprint64(&c,sizeof(payload),&b)&&h==b);
    c.major=0; assert(!mulib_contract_fingerprint64(&c,sizeof(payload),&b));
    assert(b==h);
}
static void ratequote_test(void){
    mulib_ratequote_input_t in={5,2,2,1,0,0,{0},{0},{0}};
    mulib_ratequote_result_t out={0};
    in.room_charge=eur(90000);in.mandatory_fees=eur(4000);in.taxes=eur(2000);
    assert(mulib_ratequote_normalize(&in,&out));
    assert(out.total_payable.minor==96000&&out.rounded_nightly_average.minor==19200);
    in.available=0;out.total_payable.minor=123;
    assert(!mulib_ratequote_normalize(&in,&out)&&out.total_payable.minor==123);
}
static void staymatch_test(void){
    mulib_staymatch_request_t req={0};
    mulib_staymatch_room_t rooms[3]={0};
    mulib_staymatch_bundle_t bundles[5]={0};size_t n=42;
    req.property_id=17;req.check_in_day=100;req.check_out_day=105;
    req.adults=2;req.children=2;req.max_rooms=3;
    req.require_adult_per_room=1;memcpy(req.currency,"EUR",4);req.decimals=2;
    req.max_search_nodes=100000;
    rooms[0]=(mulib_staymatch_room_t){17,1,90,110,2,2,4,1,{0}};
    rooms[1]=(mulib_staymatch_room_t){17,2,90,110,2,1,2,2,{0}};
    rooms[2]=(mulib_staymatch_room_t){17,3,90,110,4,4,4,1,{0}};
    rooms[0].full_stay_price=eur(92500);
    rooms[1].full_stay_price=eur(43750);
    rooms[2].full_stay_price=eur(102500);
    assert(mulib_staymatch_solve(&req,rooms,3,bundles,5,&n));
    assert(n>=3&&bundles[0].total_price.minor==87500&&bundles[0].selected[1]==2);
    assert(bundles[1].total_price.minor==92500);
    assert(bundles[2].total_price.minor==102500);
    req.max_search_nodes=1;n=42;bundles[0].total_price.minor=999;
    assert(!mulib_staymatch_solve(&req,rooms,3,bundles,5,&n));
    assert(n==42&&bundles[0].total_price.minor==999);
    req.max_search_nodes=100000;rooms[1].property_id=55;
    assert(!mulib_staymatch_solve(&req,rooms,3,bundles,5,&n));
}
static void compset_test(void){
    mulib_compset_offer_t offers[4]={0};
    mulib_compset_filter_t f={1000,500,2};
    mulib_compset_sample_t out[4]={0};size_t n=99;
    double prices[4]={0},weights[4]={0};
    offers[0]=(mulib_compset_offer_t){1,eur(19000),1.0,900,1};
    offers[1]=(mulib_compset_offer_t){1,eur(17500),2.0,920,1};
    offers[2]=(mulib_compset_offer_t){2,eur(18000),1.5,980,1};
    offers[3]=(mulib_compset_offer_t){3,eur(9000),1.0,200,1};
    assert(mulib_compset_build(offers,4,&f,out,4,&n));
    assert(n==2&&out[0].property_id==1&&out[0].price.minor==17500);
    assert(mulib_compset_extract(out,n,prices,weights,4));
    assert(prices[0]==17500 && weights[1]==1.5);
    f.minimum_properties=3;n=333;
    assert(!mulib_compset_build(offers,4,&f,out,4,&n)&&n==333);
}
static void pricecurve_test(void){
    mulib_pricecurve_config_t c={MULIB_PRICECURVE_POWER,0.4,0.75,90,1.8,NULL,0};
    double p=0;
    assert(mulib_pricecurve_evaluate(&c,0,&p)&&fabs(p-0.4)<1e-12);
    assert(mulib_pricecurve_evaluate(&c,90,&p)&&fabs(p-0.75)<1e-12);
    assert(mulib_pricecurve_evaluate(&c,45,&p)&&p>0.4&&p<0.75);
    c.kind=MULIB_PRICECURVE_EXPONENTIAL;c.speed=4;
    assert(mulib_pricecurve_evaluate(&c,45,&p)&&p>0.4&&p<0.75);
    {mulib_pricecurve_knot_t knots[]={{0,0.30},{7,0.40},{90,0.75}};
     c.kind=MULIB_PRICECURVE_PIECEWISE;c.knots=knots;c.knot_count=3;
     assert(mulib_pricecurve_evaluate(&c,7,&p)&&p==0.4);
     knots[1].percentile=0.9;p=345;
     assert(!mulib_pricecurve_evaluate(&c,7,&p)&&p==345);}
}
static void asof_test(void){
    mulib_asof_row_t data[]={{1,100,120,1,100},{1,100,180,2,101},{1,110,130,1,200},{2,100,115,1,300}};
    mulib_asof_row_t out={0};size_t idx=44;
    uint64_t keys[]={1,2,3};double values[3]={0};uint8_t found[3]={0};
    assert(mulib_asof_select(data,4,1,110,140,&out,&idx)==1);
    assert(out.value==200&&idx==2);
    assert(mulib_asof_select(data,4,1,100,150,&out,&idx)==1&&out.value==100);
    assert(mulib_asof_join(data,4,keys,3,100,150,values,found,3));
    assert(found[0]&&found[1]&&!found[2]&&values[1]==300);
}
static void trace_test(void){
    mulib_decisiontrace_t trace={0};mulib_decisiontrace_event_t buf[2]={0};
    mulib_decisiontrace_event_t e={MULIB_DECISIONTRACE_RULE,22,100,17500,17100,"shortcut"};
    uint64_t h=0;
    assert(mulib_decisiontrace_init(&trace,buf,2));
    assert(mulib_decisiontrace_append(&trace,&e));
    assert(mulib_decisiontrace_fingerprint64(&trace,&h)&&h);
    assert(mulib_decisiontrace_append(&trace,&e));
    assert(!mulib_decisiontrace_append(&trace,&e)&&trace.count==2);
}
static void rules_test(void){
    mulib_ruleeval_policy_t p={0};mulib_ruleeval_result_t out={0};
    p.enable_shortcut=1;p.max_shortcut_difference_minor=500;
    p.enable_floor=1;p.floor=eur(16000);
    p.enable_ceiling=1;p.ceiling=eur(20000);
    p.tick_minor=100;
    assert(mulib_ruleeval_apply(&p,eur(17500),eur(17100),&out));
    assert(out.recommended.minor==17100&&out.fired_flags&MULIB_RULEEVAL_SHORTCUT);
    p.floor=eur(19000);
    assert(mulib_ruleeval_apply(&p,eur(17500),eur(17100),&out));
    assert(out.recommended.minor==19000);
    p.floor=eur(16000);p.enable_previous=1;p.previous=eur(15000);
    p.max_change_bps=1000;
    assert(mulib_ruleeval_apply(&p,eur(17500),eur(17100),&out));
    assert(out.recommended.minor==16500&&(out.fired_flags&MULIB_RULEEVAL_CHANGE));
    p.enable_previous=0;p.floor=eur(21000);out.recommended.minor=42;
    assert(!mulib_ruleeval_apply(&p,eur(17500),eur(17100),&out));
    assert(out.recommended.minor==42);
}
static void backtest_test(void){
    mulib_backtest_row_t r[]={{10,9,12,100,110,105},{20,19,22,120,110,115}};
    mulib_backtest_summary_t s={0};
    assert(mulib_backtest_score(r,2,&s));
    assert(s.observations==2&&s.mae==10&&s.bias==0);
    r[0].prediction_known_at=11;s.mae=99;
    assert(!mulib_backtest_score(r,2,&s)&&s.mae==99);
}
static void marketquote_test(void){
    mulib_marketquote_t q={2,1,100,120,{0},{0},3,4};
    mulib_money_t spread={0},mid={0};q.bid=eur(9900);q.ask=eur(10000);
    assert(mulib_marketquote_spread_mid(&q,&spread,&mid));
    assert(spread.minor==100&&mid.minor==9950);
    q.ask=eur(9800);assert(!mulib_marketquote_valid(&q));
}
static void timeseries_test(void){
    mulib_timeseries_point_t p[]={{10,10,2},{20,40,4},{30,30,6}};
    double v=0;mulib_timeseries_point_t out={0};
    assert(mulib_timeseries_sma_asof(p,3,30,30,2,&v)&&v==4);
    assert(mulib_timeseries_latest_asof(p,3,30,30,&out)==1&&out.value==6);
    assert(!mulib_timeseries_sma_asof(p,3,30,30,3,&v));
}
static void corporate_test(void){
    mulib_corporate_actions_split_t split={2,1};
    int64_t qty=0;mulib_money_t price={0};
    assert(mulib_corporate_actions_adjust_shares(4,split,&qty)&&qty==8);
    assert(mulib_corporate_actions_adjust_price(eur(5000),split,&price)&&price.minor==2500);
    assert(mulib_corporate_actions_dividend(eur(100),4,&price)&&price.minor==400);
}
static void fx_test(void){
    mulib_fxrate_t r={{'E','U','R',0},{'J','P','Y',0},2,0,8,5,100};
    mulib_money_t out=jpy(0);
    assert(mulib_fxrate_convert(eur(1000),&r,200,200,&out));
    assert(out.minor==1600&&!memcmp(out.currency,"JPY",4));
    out.minor=42;
    assert(!mulib_fxrate_convert(eur(1000),&r,200,50,&out)&&out.minor==42);
}
static void portfolio_risk_test(void){
    mulib_portfolio_position_t p[2]={0};
    mulib_portfolio_summary_t s={0};mulib_risk_result_t res={0};
    mulib_risk_limits_t lim={0};
    p[0]=(mulib_portfolio_position_t){1,2,{0},{0}};
    p[1]=(mulib_portfolio_position_t){2,-1,{0},{0}};
    p[0].average_cost=eur(1500);p[0].mark_price=eur(2000);
    p[1].average_cost=eur(3000);p[1].mark_price=eur(2500);
    assert(mulib_portfolio_mark(p,2,&s));
    assert(s.unrealized_pnl.minor==1500&&s.gross_market_value.minor==6500);
    lim.max_gross_market_value=eur(6000);lim.max_positions=2;
    assert(mulib_risk_check(p,2,&lim,&res)&&!res.allowed&&(res.violations&1));
    lim.max_gross_market_value=eur(7000);lim.max_single_name_bps=8000;
    assert(mulib_risk_check(p,2,&lim,&res)&&res.allowed);
    lim.max_single_name_bps=6000;
    assert(mulib_risk_check(p,2,&lim,&res)&&!res.allowed&&(res.violations&4));
}
static void orderbook_test(void){
    mulib_orderbook_level_t levels[5];mulib_orderbook_t b={0};
    mulib_orderbook_level_t best={0};
    assert(mulib_orderbook_init(&b,levels,5));
    assert(mulib_orderbook_apply(&b,1,(mulib_orderbook_level_t){MULIB_ORDERBOOK_ASK,10000,2}));
    assert(mulib_orderbook_apply(&b,2,(mulib_orderbook_level_t){MULIB_ORDERBOOK_BID,9900,3}));
    assert(!mulib_orderbook_apply(&b,3,(mulib_orderbook_level_t){MULIB_ORDERBOOK_BID,10100,3}));
    assert(b.last_sequence==2&&b.count==2);
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_BID,&best)==1&&best.price_minor==9900);
    assert(mulib_orderbook_apply(&b,3,(mulib_orderbook_level_t){MULIB_ORDERBOOK_ASK,10000,0}));
    assert(mulib_orderbook_best(&b,MULIB_ORDERBOOK_ASK,&best)==0);
}
static void orderintent_test(void){
    mulib_orderintent_t i={5,8,MULIB_ORDERINTENT_BUY,2,{0},100,200};
    mulib_orderintent_policy_t p={5,{0},100,1,1};mulib_money_t out=eur(0);
    i.limit_price=eur(10000);p.max_notional=eur(20000);
    assert(mulib_orderintent_validate(&i,&p,150,&out)&&out.minor==20000);
    out.minor=42;
    assert(!mulib_orderintent_validate(&i,&p,200,&out)&&out.minor==42);
}
int main(void){
    contract_test();ratequote_test();staymatch_test();compset_test();pricecurve_test();
    asof_test();trace_test();rules_test();backtest_test();marketquote_test();
    timeseries_test();corporate_test();fx_test();portfolio_risk_test();
    orderbook_test();orderintent_test();
    puts("FinPRED kernel tests passed (17 headers)");return 0;
}
