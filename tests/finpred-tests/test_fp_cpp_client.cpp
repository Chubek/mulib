/* These declaration-only calls link to the common genuine C11 object. */
#include "mulib-asof.h"
#include "mulib-backtest.h"
#include "mulib-compset.h"
#include "mulib-contract.h"
#include "mulib-corporate-actions.h"
#include "mulib-decisiontrace.h"
#include "mulib-fxrate.h"
#include "mulib-marketquote.h"
#include "mulib-orderbook.h"
#include "mulib-orderintent.h"
#include "mulib-portfolio.h"
#include "mulib-pricecurve.h"
#include "mulib-ratequote.h"
#include "mulib-risk.h"
#include "mulib-ruleeval.h"
#include "mulib-staymatch.h"
#include "mulib-timeseries.h"
#include <cassert>

int main(){
    mulib_money_t eur{};
    assert(mulib_money_make(&eur,250,"EUR",2));

    const uint64_t key=12;
    mulib_asof_snapshot_entry_t snapshot{};
    assert(mulib_asof_snapshot(nullptr,0,nullptr,&key,1,100,100,&snapshot,1));
    assert(snapshot.status==MULIB_ASOF_MISSING&&snapshot.row.key==key);
    mulib_backtest_summary_t score{};
    assert(!mulib_backtest_score_replay(nullptr,0,100,&score));

    mulib_decisiontrace_event_t buffer[1]{};
    mulib_decisiontrace_t trace{};
    size_t size=0;
    assert(mulib_decisiontrace_init(&trace,buffer,1));
    assert(mulib_decisiontrace_encode_v1(&trace,nullptr,0,&size)&&size==13);
    const mulib_ruleeval_percentile_shortcut_t shortcut{
        5000,1000,MULIB_RULEEVAL_BELOW_THRESHOLD,MULIB_RULEEVAL_CLAMP_TO_FLOOR
    };
    uint16_t lower=0;
    assert(mulib_ruleeval_shortcut_percentile(&shortcut,&lower)&&lower==4000);
    assert(!mulib_marketquote_usable_at(nullptr,100,5,1));
    assert(!mulib_timeseries_validate(nullptr,0));

    int64_t shares=0;
    assert(mulib_corporate_actions_adjust_shares(9,{2,3},&shares)&&shares==6);
    assert(!mulib_fxrate_convert_asof(eur,nullptr,100,100,10,&eur)&&eur.minor==250);
    assert(!mulib_portfolio_gross(nullptr,0,&eur));
    assert(!mulib_risk_check(nullptr,0,nullptr,nullptr));
    assert(!mulib_orderintent_validate(nullptr,nullptr,100,&eur));
    mulib_orderbook_level_t levels[1]{};
    mulib_orderbook_t book{};
    assert(mulib_orderbook_init(&book,levels,1));
    assert(mulib_orderbook_load_snapshot(&book,12,nullptr,0));
    assert(mulib_orderbook_apply(&book,13,{MULIB_ORDERBOOK_BID,100,2}));

    assert(!mulib_contract_valid(nullptr,0));
    assert(!mulib_ratequote_normalize(nullptr,nullptr));
    assert(!mulib_staymatch_solve(nullptr,nullptr,0,nullptr,0,nullptr));
    assert(!mulib_compset_build(nullptr,0,nullptr,nullptr,0,nullptr));
    assert(!mulib_pricecurve_evaluate(nullptr,0,nullptr));
}
