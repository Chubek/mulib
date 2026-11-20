/* Synthetic point-in-time FinPRED pricing decision; no publication or I/O
 * beyond printing the example's result. Build via make check-finpred. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define MULIB_IMPL_CONTRACT
#define MULIB_IMPL_SHA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_RATEQUOTE
#define MULIB_IMPL_STAYMATCH
#define MULIB_IMPL_COMPSET
#define MULIB_IMPL_PRICECURVE
#define MULIB_IMPL_RULEEVAL
#define MULIB_IMPL_ASOF
#define MULIB_IMPL_DECISIONTRACE
#include "mulib-contract.h"
#include "mulib-json.h"
#include "mulib-statsproc.h"
#include "mulib-ratequote.h"
#include "mulib-staymatch.h"
#include "mulib-compset.h"
#include "mulib-pricecurve.h"
#include "mulib-ruleeval.h"
#include "mulib-asof.h"
#include "mulib-decisiontrace.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static mulib_money_t eur(int64_t cents){
    mulib_money_t m;
    assert(mulib_money_make(&m,cents,"EUR",2));
    return m;
}
static uint64_t recommend(const mulib_contract_t *contract,const mulib_asof_row_t *market,
                         size_t market_count,int64_t decision_at,int print){
    const uint64_t properties[]={1,2,3};
    const uint8_t ages[]={6,11};
    mulib_asof_snapshot_entry_t snapshot[3];
    mulib_staymatch_request_t req={0};
    mulib_compset_offer_t rivals[3]={0};
    mulib_compset_sample_t samples[3]={0};
    mulib_compset_reason_t reasons[3];
    mulib_compset_filter_t filter={decision_at,500,3};
    mulib_pricecurve_config_t curve={MULIB_PRICECURVE_LINEAR,0.5,0.75,90,1.0,NULL,0};
    mulib_ruleeval_policy_t policy={0};
    mulib_ruleeval_result_t recommendation={0};
    mulib_ruleeval_percentile_shortcut_t shortcut={
        5000,1000,MULIB_RULEEVAL_BELOW_THRESHOLD,MULIB_RULEEVAL_CLAMP_TO_FLOOR
    };
    mulib_decisiontrace_event_t storage[6];
    mulib_decisiontrace_t trace;
    mulib_json_t *stay;
    uint64_t context, fingerprint;
    uint16_t lower_bps;
    size_t i,matched,selected;
    double observed[3],weights[3],p,market_p,market_lower;
    mulib_ruleeval_percentile_quote_t high,low;
    assert(mulib_contract_usable_at(contract,decision_at,4096));
    assert(!strcmp(contract->schema_id,"hospitality.stay.v1") && contract->major==1);
    assert(mulib_contract_fingerprint64(contract,4096,&context));
    stay=mulib_json_parse(contract->data,contract->data_size,NULL);
    /* Interpret only this fixture's schema; do not coerce arbitrary JSON. */
    assert(stay&&mulib_json_typeof(stay)==MULIB_JSON_OBJECT&&mulib_json_size(stay)==5);
    assert(mulib_json_numeric(mulib_json_get(stay,"check_in"))==100);
    assert(mulib_json_numeric(mulib_json_get(stay,"check_out"))==105);
    assert(mulib_json_numeric(mulib_json_get(stay,"adults"))==2);
    assert(mulib_json_numeric(mulib_json_get(stay,"children"))==2);
    const char *currency=mulib_json_text(mulib_json_get(stay,"currency"),NULL);
    assert(currency&&!strcmp(currency,"EUR"));
    mulib_json_destroy(stay);
    assert(mulib_asof_snapshot(market,market_count,NULL,properties,3,
           decision_at,decision_at,snapshot,3));
    assert(mulib_decisiontrace_init(&trace,storage,6));
    req.check_in_day=100;req.check_out_day=105;
    req.adults=2;req.children=2;req.max_rooms=3;
    req.require_adult_per_room=1;req.max_search_nodes=10000;
    memcpy(req.currency,"EUR",4);req.decimals=2;
    /* Three independent properties: family room, two doubles, apartment. */
    for(i=0;i<3;++i){
        unsigned doubles=i==1;
        double price=snapshot[i].row.value;
        assert(snapshot[i].status==MULIB_ASOF_PRESENT && price>=0 &&
               price<=9007199254740992.0 && trunc(price)==price);
        const mulib_ratequote_terms_t terms={
            .check_in_day=100,.check_out_day=105,.observed_at=snapshot[i].row.known_at,
            .book_from=0,.book_until=1000,.source_id=properties[i],
            .meal=MULIB_RATEQUOTE_ROOM_ONLY,.cancellation=MULIB_RATEQUOTE_NONREFUNDABLE,
            .child_ages=ages,.child_age_count=doubles?1:2
        };
        mulib_ratequote_input_t quote={
            .nights=5,.adults=doubles?1:2,.children=doubles?1:2,.available=1,
            .room_charge=eur((int64_t)price),
            .mandatory_fees=eur(0),.taxes=eur(0)
        };
        mulib_ratequote_result_t normalized;
        mulib_staymatch_dated_room_t room={
            .room={properties[i],1,90,110,2,doubles?1:2,doubles?2:4,doubles?2:1,{0}},
            .quoted_check_in_day=100,.quoted_check_out_day=105,
            .book_from=0,.book_until=1000,.min_adults=1,.min_child_age=0,.max_child_age=17
        };
        mulib_staymatch_bundle_t result;
        mulib_money_t nightly;
        assert(mulib_ratequote_normalize_dated(&quote,&terms,100,105,decision_at,&normalized));
        room.room.full_stay_price=normalized.total_payable;
        req.property_id=properties[i];
        assert(mulib_staymatch_solve_dated(&req,decision_at,ages,2,&room,1,&result,1,&matched));
        assert(matched==1&&result.room_count==(doubles?2:1));
        assert(mulib_money_scale(result.total_price,1,5,&nightly));
        rivals[i]=(mulib_compset_offer_t){properties[i],nightly,1.0,snapshot[i].row.known_at,1};
        mulib_decisiontrace_event_t event={MULIB_DECISIONTRACE_OBSERVATION,context,
            decision_at,result.total_price.minor,nightly.minor,"comparable-nightly"};
        assert(mulib_decisiontrace_append(&trace,&event));
    }
    assert(mulib_compset_build_diagnosed(rivals,3,&filter,samples,3,&selected,reasons,3));
    for(i=0;i<3;++i)assert(reasons[i]==MULIB_COMPSET_SELECTED);
    assert(mulib_compset_extract(samples,selected,observed,weights,3));
    assert(mulib_pricecurve_evaluate(&curve,0,&p)); /* P50 at arrival */
    assert(mulib_ruleeval_shortcut_percentile(&shortcut,&lower_bps));
    assert(mulib_statsproc_quantile(observed,selected,p,&market_p));
    assert(mulib_statsproc_quantile(observed,selected,lower_bps/10000.0,&market_lower));
    high=(mulib_ruleeval_percentile_quote_t){(uint16_t)lround(p*10000),eur((int64_t)llround(market_p))};
    low=(mulib_ruleeval_percentile_quote_t){lower_bps,eur((int64_t)llround(market_lower))};
    policy.enable_shortcut=1;policy.max_shortcut_difference_minor=500;
    policy.enable_floor=1;policy.floor=eur(17000);
    policy.enable_ceiling=1;policy.ceiling=eur(25000);
    policy.tick_minor=100;
    assert(mulib_ruleeval_apply_percentiles(&policy,&shortcut,high,low,&recommendation));
    mulib_decisiontrace_event_t final={MULIB_DECISIONTRACE_FINAL,context,decision_at,
        high.amount.minor,recommendation.recommended.minor,"shortcut-p40"};
    assert(recommendation.recommended.minor==18300 &&
           recommendation.fired_flags==MULIB_RULEEVAL_SHORTCUT);
    assert(mulib_decisiontrace_append(&trace,&final));
    assert(mulib_decisiontrace_fingerprint64(&trace,&fingerprint));
    if(print)
        printf("%zu properties; P50=%" PRId64 "; P40=%" PRId64
               "; recommended=%" PRId64 " EUR minor/night; trace=%016" PRIx64 "\n",
               selected,high.amount.minor,low.amount.minor,recommendation.recommended.minor,fingerprint);
    return fingerprint;
}

int main(void){
    static const char stay[]="{\"check_in\":100,\"check_out\":105,\"adults\":2,\"children\":2,\"currency\":\"EUR\"}";
    const mulib_contract_t contract={"stay","hospitality.stay","hospitality.stay.v1",
        "synthetic-fixture",1,0,100,100,500,0,stay,sizeof stay-1};
    const mulib_asof_row_t market[]={
        {1,150,150,1,92500},{2,150,150,1,43750},{3,150,150,1,102500},
        {2,150,210,2,85000} /* visible only after this decision */
    };
    assert(recommend(&contract,market,4,200,1)==recommend(&contract,market,3,200,0));
    return 0;
}
