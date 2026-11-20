/* Focused Contract, RateQuote, CompSet and PriceCurve regressions.
 * Build with the canonical include directory and -DMULIB_IMPL_MONEY. */
#define MULIB_IMPL_SHA
#define MULIB_IMPL_CONTRACT
#define MULIB_IMPL_RATEQUOTE
#define MULIB_IMPL_PRICECURVE
#define MULIB_IMPL_STATSPROC
#include "mulib-contract.h"
#include "mulib-ratequote.h"
#include "mulib-pricecurve.h"
#include "mulib-statsproc.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int oom;
static void *compset_malloc(size_t n){return oom?NULL:malloc(n);}
#define malloc compset_malloc
#define MULIB_IMPL_COMPSET
#include "mulib-compset.h"
#undef malloc

static mulib_money_t eur(int64_t minor){
    mulib_money_t m;
    assert(mulib_money_make(&m,minor,"EUR",2));
    return m;
}
static void contract_cases(void){
    const unsigned char payload[]={0,255,1,128};
    const unsigned char expected[]={
        0x3f,0x2f,0x4e,0x54,0x1b,0xaf,0x95,0x87,0x45,0xfb,0x05,0x87,0x31,0xd6,0xbb,0x34,
        0x53,0xed,0xd5,0x05,0x12,0x39,0xd4,0x84,0x0c,0x7a,0x36,0x43,0x96,0xc9,0xd2,0xfb
    }; /* Independently generated with Python hashlib over the documented format. */
    mulib_contract_t c={"dataset-1","hospitality.offer","finpred.v1","fixture",1,4,10,5,30,7,
        payload,sizeof(payload)};
    unsigned char out[36],other[32];
    char id[129];
    size_t i;
    uint64_t h=0;
    memset(out,0xa5,sizeof(out));
    assert(mulib_contract_digest_sha256(&c,sizeof(payload),out,sizeof(out)));
    assert(!memcmp(out,expected,sizeof(expected))&&out[32]==0xa5&&out[35]==0xa5);
    assert(mulib_contract_fingerprint64(&c,sizeof(payload),&h)&&h==UINT64_C(0xe7b03c556911451f));
    c.quality_flags++;
    assert(mulib_contract_digest_sha256(&c,sizeof(payload),other,sizeof(other)));
    assert(memcmp(out,other,sizeof(other)));
    c.data=NULL;c.data_size=0;
    assert(mulib_contract_digest_sha256(&c,0,other,sizeof(other)));
    c.data=payload;c.data_size=sizeof(payload);
    assert(!mulib_contract_usable_at(&c,9,sizeof(payload)));
    assert(mulib_contract_usable_at(&c,10,sizeof(payload)));
    assert(!mulib_contract_usable_at(&c,30,sizeof(payload)));
    assert(!mulib_contract_digest_sha256(&c,3,out,sizeof(out)));
    assert(!mulib_contract_digest_sha256(&c,sizeof(payload),out,31));
    c.id=id;
    memset(id,'x',127);id[127]=0;
    assert(mulib_contract_valid(&c,sizeof(payload)));
    id[127]='x';id[128]=0;
    assert(!mulib_contract_valid(&c,sizeof(payload)));
    for(i=1;i<256;++i){
        int ascii_ok=(i>='A'&&i<='Z')||(i>='a'&&i<='z')||(i>='0'&&i<='9')||
                      i=='.'||i=='_'||i=='-'||i==':';
        ((unsigned char *)id)[0]=(unsigned char)i;id[1]=0;
        assert(mulib_contract_valid(&c,sizeof(payload))==ascii_ok);
        if(!ascii_ok)assert(!mulib_contract_digest_sha256(&c,sizeof(payload),out,sizeof(out)));
    }
    assert(!memcmp(out,expected,sizeof(expected))&&out[32]==0xa5&&out[35]==0xa5);
}

static void ratequote_cases(void){
    mulib_ratequote_input_t in={2,2,2,1,0,0,{0},{0},{0}};
    mulib_ratequote_result_t out;
    mulib_money_t nightly[]={eur(40),eur(41)};
    uint8_t ages[]={0,17};
    mulib_ratequote_terms_t t={-1,1,9,10,12,1,MULIB_RATEQUOTE_BREAKFAST,
        MULIB_RATEQUOTE_FREE_UNTIL,11,ages,2,nightly,2};
    unsigned char saved[sizeof(out)];
    uint64_t s=UINT64_C(42);
    unsigned i,j,k;
    in.room_charge=eur(81);in.mandatory_fees=eur(5);in.taxes=eur(4);
    assert(mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    assert(out.total_payable.minor==90&&out.rounded_nightly_average.minor==45&&out.nights==2);
    assert(mulib_ratequote_normalize_dated(&in,&t,-1,1,11,&out));
    memcpy(saved,&out,sizeof(out));
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,12,&out));
    assert(!mulib_ratequote_normalize_dated(&in,&t,-2,0,10,&out));
    t.check_in_day=INT64_MIN;t.check_out_day=INT64_MAX;
    assert(!mulib_ratequote_normalize_dated(&in,&t,INT64_MIN,INT64_MAX,10,&out));
    t.check_in_day=-1;t.check_out_day=1;
    t.observed_at=11;
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    t.observed_at=9;ages[0]=18;
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    ages[0]=0;t.cancellation=MULIB_RATEQUOTE_NONREFUNDABLE;
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    t.free_cancel_before=0;nightly[0].decimals=0;
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    nightly[0]=eur(40);in.taxes_included=in.fees_included=1;in.taxes=eur(80);
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    in.taxes=eur(4);in.taxes_included=in.fees_included=0;
    nightly[0]=eur(39);
    assert(!mulib_ratequote_normalize_dated(&in,&t,-1,1,10,&out));
    assert(!memcmp(saved,&out,sizeof(out)));

    /* Exact independent unsigned oracle: arbitrary nonnegative room, fee and
     * tax values <= INT64_MAX/2. uint64_t fits their full sum, even if Money
     * must reject it. Quotient + remainder avoid overflow and floating point. */
    for(i=0;i<30000;++i){
        mulib_money_t *parts[]={&in.room_charge,&in.mandatory_fees,&in.taxes};
        uint64_t total,v[3],divisor,rem;
        for(j=0;j<3;++j){
            s=s*UINT64_C(6364136223846793005)+1;
            v[j]=s>>2;
            *parts[j]=eur((int64_t)v[j]);
        }
        divisor=1+s%365;in.nights=(uint32_t)divisor;
        for(k=0;k<4;++k){
            in.fees_included=(uint8_t)(k&1);in.taxes_included=(uint8_t)(k>>1);
            total=v[0]+(in.fees_included?0:v[1])+(in.taxes_included?0:v[2]);
            if(total>INT64_MAX){
                memcpy(saved,&out,sizeof(out));
                assert(!mulib_ratequote_normalize(&in,&out)&&!memcmp(saved,&out,sizeof(out)));
            }else{
                rem=total%divisor;
                assert(mulib_ratequote_normalize(&in,&out));
                assert(out.total_payable.minor==(int64_t)total&&out.remainder_minor==(int64_t)rem);
                assert(out.rounded_nightly_average.minor==(int64_t)(total/divisor+(rem*2>=divisor)));
            }
        }
    }
    in.nights=365;in.fees_included=in.taxes_included=0;in.room_charge=eur(INT64_MAX);
    in.mandatory_fees=eur(0);in.taxes=eur(0);
    assert(mulib_ratequote_normalize(&in,&out)&&out.total_payable.minor==INT64_MAX);
    memcpy(saved,&out,sizeof(out));
    in.taxes=eur(1);
    assert(!mulib_ratequote_normalize(&in,&out)&&!memcmp(saved,&out,sizeof(out)));
}

static void compset_cases(void){
    mulib_compset_offer_t o[]={
        {2,{0},1,95,1},{1,{0},2,90,1},{1,{0},3,90,1},{1,{0},1,89,1},
        {3,{0},1,100,1},{4,{0},1,89,1},{5,{0},1,101,1},{0,{0},0,0,0}
    };
    mulib_compset_filter_t f={100,10,3};
    mulib_compset_sample_t out[4];
    mulib_compset_reason_t reasons[8];
    unsigned char saved[sizeof(out)],why[sizeof(reasons)];
    double p[3],w[3],a,b;
    size_t n=0,i;
    for(i=0;i<7;++i)o[i].effective_nightly_price=eur(100);
    o[0].effective_nightly_price=eur(200);o[4].effective_nightly_price=eur(300);
    memset(out,0,sizeof(out));
    assert(mulib_compset_build_diagnosed(o,8,&f,out,4,&n,reasons,8)&&n==3);
    assert(reasons[0]==MULIB_COMPSET_SELECTED&&reasons[1]==MULIB_COMPSET_SELECTED);
    assert(reasons[2]==MULIB_COMPSET_PROPERTY_DUPLICATE&&reasons[3]==MULIB_COMPSET_STALE);
    assert(reasons[4]==MULIB_COMPSET_SELECTED&&reasons[5]==MULIB_COMPSET_STALE);
    assert(reasons[6]==MULIB_COMPSET_FUTURE&&reasons[7]==MULIB_COMPSET_INACTIVE);
    assert(mulib_compset_extract(out,n,p,w,3)&&p[0]==100&&p[1]==200&&p[2]==300&&w[0]==2);
    assert(mulib_statsproc_quantile(p,n,0.25,&a)&&a==150);
    assert(mulib_statsproc_weighted_quantile(p,w,n,0.25,&b)&&b==100);
    {
        mulib_compset_offer_t swap=o[1];o[1]=o[2];o[2]=swap;
        assert(mulib_compset_build(o,8,&f,out,4,&n)&&out[0].weight==2);
    }
    memcpy(saved,out,sizeof(out));memcpy(why,reasons,sizeof(reasons));n=7;
    oom=1;
    assert(!mulib_compset_build_diagnosed(o,8,&f,out,4,&n,reasons,8));
    oom=0;
    assert(!mulib_compset_build_diagnosed(o,8,&f,out,2,&n,reasons,8));
    assert(!mulib_compset_build_diagnosed(o,8,&f,out,4,&n,reasons,7));
    o[6].relevance_weight=NAN;
    assert(!mulib_compset_build_diagnosed(o,8,&f,out,4,&n,reasons,8));
    assert(n==7&&!memcmp(why,reasons,sizeof(reasons))&&!memcmp(saved,out,sizeof(out)));
    out[2].price=eur(INT64_C(9007199254740992));
    assert(mulib_compset_extract(out,3,p,w,3)&&p[2]==9007199254740992.);
    ++out[2].price.minor;p[0]=42;w[0]=43;
    assert(!mulib_compset_extract(out,3,p,w,3)&&p[0]==42&&w[0]==43);
    --out[2].price.minor;
    assert(!mulib_compset_extract(out,3,p,p,3)&&p[0]==42);
}

static void curve_cases(void){
    double y,prev;
    unsigned kind,i,j;
    const double speeds[]={0,DBL_TRUE_MIN,1e-8,.2,1,8,20};
    mulib_pricecurve_config_t c={MULIB_PRICECURVE_LINEAR,0.15,0.95,80,1,NULL,0};
    mulib_pricecurve_knot_t knots[]={{0,0.2},{5,0.35},{9,0.35},{100,0.9}};
    for(kind=0;kind<=2;++kind)for(j=0;j<sizeof(speeds)/sizeof(speeds[0]);++j){
        c.kind=(mulib_pricecurve_kind_t)kind;c.speed=speeds[j];prev=0;
        if(kind==MULIB_PRICECURVE_POWER && c.speed==0)continue;
        for(i=0;i<=2000;++i){
            double u=(double)i/2000;
            long double t=(long double)u,expected;
            assert(mulib_pricecurve_evaluate(&c,u*c.horizon_days,&y)&&y>=prev);
            if(kind==MULIB_PRICECURVE_POWER)t=powl(t,(long double)c.speed);
            if(kind==MULIB_PRICECURVE_EXPONENTIAL && c.speed>=DBL_MIN)
                t=expm1l(-t*c.speed)/expm1l(-(long double)c.speed);
            expected=c.near_percentile+((long double)c.far_percentile-c.near_percentile)*t;
            assert(fabsl(y-expected)<3e-13L);
            prev=y;
        }
        assert(mulib_pricecurve_evaluate(&c,0,&y)&&y==c.near_percentile);
        assert(mulib_pricecurve_evaluate(&c,80,&y)&&y==c.far_percentile);
        assert(mulib_pricecurve_evaluate(&c,DBL_MAX,&y)&&y==c.far_percentile);
    }
    c.kind=MULIB_PRICECURVE_POWER;c.horizon_days=DBL_MAX;c.speed=0.5;
    c.near_percentile=0;c.far_percentile=1;
    assert(mulib_pricecurve_evaluate(&c,DBL_TRUE_MIN,&y)&&y>0&&y<DBL_MIN);
    c.kind=MULIB_PRICECURVE_PIECEWISE;c.knots=knots;c.knot_count=4;
    prev=0;
    for(i=0;i<1200;++i){
        assert(mulib_pricecurve_evaluate(&c,i/10.,&y)&&y>=prev);
        prev=y;
    }
    for(i=1;i<4;++i){
        double x=knots[i].days_to_event,p=knots[i].percentile;
        assert(mulib_pricecurve_evaluate(&c,x,&y)&&y==p);
        assert(mulib_pricecurve_evaluate(&c,nextafter(x,-INFINITY),&y)&&y<=p&&p-y<1e-14);
        assert(mulib_pricecurve_evaluate(&c,nextafter(x,INFINITY),&y)&&y>=p&&y-p<1e-14);
    }
    y=42;knots[3].days_to_event=9;
    assert(!mulib_pricecurve_evaluate(&c,0,&y)&&y==42);
    knots[3].days_to_event=100;knots[2].percentile=0.3;
    assert(!mulib_pricecurve_evaluate(&c,999,&y)&&y==42);
    assert(!mulib_pricecurve_evaluate(&c,-1,&y)&&y==42);
    assert(!mulib_pricecurve_evaluate(&c,NAN,&y)&&y==42);
}

int main(void){
    contract_cases();
    ratequote_cases();
    compset_cases();
    curve_cases();
    puts("P0 Contract, RateQuote, CompSet, PriceCurve focused tests passed");
    return 0;
}
