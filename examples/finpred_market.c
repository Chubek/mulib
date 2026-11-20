/* Synthetic FinPRED market data example. It never connects to a venue,
 * authorizes an order, or submits one. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_BACKTEST
#define MULIB_IMPL_MARKETQUOTE
#define MULIB_IMPL_TIMESERIES
#define MULIB_IMPL_CORPORATE_ACTIONS
#define MULIB_IMPL_FXRATE
#define MULIB_IMPL_PORTFOLIO
#define MULIB_IMPL_RISK
#define MULIB_IMPL_ORDERBOOK
#define MULIB_IMPL_ORDERINTENT
#include "mulib-backtest.h"
#include "mulib-marketquote.h"
#include "mulib-timeseries.h"
#include "mulib-corporate-actions.h"
#include "mulib-fxrate.h"
#include "mulib-risk.h"
#include "mulib-orderbook.h"
#include "mulib-orderintent.h"
#include <assert.h>
#include <stdio.h>

static mulib_money_t eur(int64_t minor)
{
    mulib_money_t m;
    assert(mulib_money_make(&m, minor, "EUR", 2));
    return m;
}

int main(void)
{
    mulib_marketquote_t quote = {1, 1, 100000000, 110000000, {0}, {0}, 10, 12};
    mulib_money_t spread, mid, converted, notional;
    mulib_orderbook_level_t levels[2], best;
    mulib_orderbook_t book;
    int64_t adjusted_shares;
    mulib_portfolio_summary_t portfolio;
    mulib_risk_result_t risk;
    mulib_backtest_summary_t score;
    double sma;

    quote.bid = eur(1000);
    quote.ask = eur(1020);
    assert(mulib_marketquote_usable_at(&quote, 120000000, 50000000, 10));
    assert(mulib_marketquote_spread_mid(&quote, &spread, &mid));
    assert(spread.minor == 20 && mid.minor == 1010);
    assert(mulib_orderbook_init(&book, levels, 2));
    assert(mulib_orderbook_apply(&book, 1, (mulib_orderbook_level_t){
        MULIB_ORDERBOOK_BID, quote.bid.minor, quote.bid_size
    }));
    assert(mulib_orderbook_apply(&book, 2, (mulib_orderbook_level_t){
        MULIB_ORDERBOOK_ASK, quote.ask.minor, quote.ask_size
    }));
    assert(mulib_orderbook_best(&book, MULIB_ORDERBOOK_ASK, &best) == 1);
    assert(best.price_minor == quote.ask.minor);

    const mulib_timeseries_point_t history[] = {
        {90000000, 90000000, 990}, {100000000, 110000000, 1010}, {130000000, 140000000, 1020}
    };
    assert(mulib_timeseries_sma_asof(history, 3, 120000000, 120000000, 2, &sma) && sma == 1000);
    const mulib_backtest_replay_row_t holdout[] = {{{120, 120, 140, sma, 1020, 990}, 110, 90, 130}};
    assert(mulib_backtest_score_replay(holdout, 1, 140, &score) && score.mae == 20);

    const mulib_corporate_actions_split_t split = {2, 1};
    assert(mulib_corporate_actions_adjust_shares(5, split, &adjusted_shares) && adjusted_shares == 10);
    const mulib_portfolio_position_t positions[] = {{1, adjusted_shares, eur(1000), mid}};
    assert(mulib_portfolio_mark(positions, 1, &portfolio) && portfolio.unrealized_pnl.minor == 100);
    const mulib_risk_limits_t limits = {eur(20000), 3, 0};
    assert(mulib_risk_check(positions, 1, &limits, &risk) && risk.allowed);
    const mulib_fxrate_t fx = {"EUR", "USD", 2, 2, 11, 10, 100};
    assert(mulib_fxrate_convert_asof(risk.observed_gross_market_value, &fx, 110, 120, 50, &converted));
    assert(converted.minor == 11110);

    const mulib_orderintent_t intent = {1, 1, MULIB_ORDERINTENT_BUY, 2, quote.ask, 110, 130};
    const mulib_orderintent_policy_t policy = {10, eur(10000), 10, 1, 0};
    assert(mulib_orderintent_validate(&intent, &policy, 120, &notional) && notional.minor == 2040);
    puts("FinPRED market snapshot and offline order preflight verified.");
    return 0;
}
