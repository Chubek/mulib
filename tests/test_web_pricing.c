#include "fixtures/web_primitives_impl.h"
#include <assert.h>
#include <float.h>

static mulib_price_observation_t
observation (int64_t time, int64_t minor)
{
  mulib_price_observation_t o = { 0 };
  strcpy (o.product, "widget");
  strcpy (o.merchant, "shop");
  o.observed_at = time;
  assert (mulib_money_make (&o.price, minor, "USD", 2));
  return o;
}

static void
money (void)
{
  mulib_money_t a, b, out;
  assert (mulib_money_parse ("-92233720368547758.08", "USD", 2, &a));
  assert (a.minor == INT64_MIN);
  assert (mulib_money_scale (a, INT64_MAX, INT64_MAX, &out)
          && out.minor == INT64_MIN);
  assert (mulib_money_make (&b, INT64_MAX, "USD", 2));
  assert (mulib_money_scale (b, INT64_MAX, INT64_MAX, &out)
          && out.minor == INT64_MAX);
  assert (mulib_money_add (a, b, &out) && out.minor == -1);
  out = b;
  assert (!mulib_money_scale (b, 2, 1, &out) && out.minor == INT64_MAX);
  assert (!mulib_money_sub (a, b, &out) && out.minor == INT64_MAX);
  assert (!mulib_money_parse ("92233720368547758.08", "USD", 2, &out));
  assert (!mulib_money_parse ("1.", "USD", 2, &out));
  assert (!mulib_money_parse ("1.001", "USD", 2, &out));
  assert (!mulib_money_make (&out, 1, "usd", 2));
  a.currency[0] = '?';
  assert (!mulib_money_scale (a, 1, 1, &out));
  for (int64_t v = -80; v <= 80; v++)
    for (int64_t n = 0; n <= 80; n++)
      for (int64_t d = 1; d <= 80; d++)
        {
          assert (mulib_money_make (&a, v, "USD", 2));
          assert (mulib_money_scale (a, n, d, &out));
          int64_t magnitude = v < 0 ? -v : v;
          int64_t expected = (magnitude * n + d / 2) / d;
          assert (out.minor == (v < 0 ? -expected : expected));
        }
  mulib_price_normalize_input_t in = { 0 };
  assert (mulib_money_make (&in.base, 1000, "USD", 2));
  assert (mulib_money_make (&in.shipping, 100, "USD", 2));
  in.discount_bp = 1000;
  in.tax_bp = 2000;
  in.quantity = 2;
  assert (mulib_price_normalize (&in, &a, &b) && a.minor == 1180
          && b.minor == 590);
  assert (!mulib_price_normalize (&in, &a, &a));
  in.quantity = 0;
  assert (!mulib_price_normalize (&in, &a, &b));
}

static void
history (void)
{
  mulib_price_history_t h = { 0 };
  h.limit = 3;
  mulib_price_observation_t a = observation (20, 300),
                            b = observation (0, 100);
  assert (mulib_price_history_add (&h, &a));
  assert (mulib_price_history_add (&h, &b));
  a.observed_at = 10;
  a.price.minor = 200;
  assert (mulib_price_history_add (&h, &a));
  a.price.minor = 220;
  assert (mulib_price_history_add (&h, &a));
  assert (h.len == 3);
  assert (h.items[0].observed_at == 0 && h.items[1].price.minor == 220);
  assert (mulib_price_history_add (&h, h.items + 1));
  assert (mulib_price_history_range (&h, 10, 20, NULL, 0) == 2);
  mulib_price_point_t p[5];
  assert (mulib_price_timeseries_build (&h, 0, 5, 5, p));
  assert (p[1].count == 0);
  assert (mulib_price_timeseries_interpolate (p, 5));
  assert (p[1].value == 160 && p[3].value == 260 && !p[1].observed);
  mulib_price_point_t irregular[]
      = { { 0, 0, 1, 1 }, { 1, 0, 0, 0 }, { 10, 100, 1, 1 } };
  assert (mulib_price_timeseries_interpolate (irregular, 3)
          && irregular[1].value == 10);
  irregular[2].time = 0;
  assert (!mulib_price_timeseries_interpolate (irregular, 3));
  assert (!mulib_price_timeseries_build (&h, INT64_MAX, 1, 2, p));
  strcpy (h.items[1].product, "other");
  assert (!mulib_price_timeseries_build (&h, 0, 5, 5, p));
  mulib_price_monitor_event_t event;
  a = b;
  a.price.minor = 120;
  assert (mulib_price_monitor_compare (&b, &a, .1, &event)
          && event == MULIB_PRICE_MONITOR_UP);
  a.flags = 1;
  assert (mulib_price_monitor_compare (&b, &a, .1, &event)
          && event == MULIB_PRICE_MONITOR_STOCK);
  a.flags = 8;
  assert (!mulib_price_observation_validate (&a));
  mulib_price_history_free (&h);
}

static void
statistics (void)
{
  double series[] = { 10, 20, 30, 40 }, value = 0, spread = 0;
  assert (mulib_price_forecast (series, 4, 2, MULIB_PRICE_FORECAST_DRIFT, 0, 0,
                                &value)
          && value == 60);
  assert (mulib_price_forecast (series, 4, 3, MULIB_PRICE_FORECAST_SEASONAL, 0,
                                2, &value)
          && value == 30);
  assert (mulib_price_forecast (series, 4, 1, MULIB_PRICE_FORECAST_NAIVE, 0, 0,
                                &value)
          && value == 40);
  assert (mulib_price_forecast (series, 4, 1, MULIB_PRICE_FORECAST_EMA, .5, 0,
                                &value)
          && value == 31.25);
  assert (mulib_price_forecast_mae (series, 4, MULIB_PRICE_FORECAST_DRIFT, 0,
                                    0, &value)
          && value == 0);
  series[3] = NAN;
  value = 77;
  assert (!mulib_price_forecast_mae (series, 4, MULIB_PRICE_FORECAST_NAIVE, 0,
                                     0, &value)
          && value == 77);
  series[3] = 40;
  double scores[] = { .25, .75 }, weights[] = { DBL_MAX, DBL_MAX };
  assert (mulib_price_confidence_combine (scores, weights, 2, &value)
          && value == .5);
  weights[0] = -1;
  assert (!mulib_price_confidence_combine (scores, weights, 2, &value));
  assert (mulib_price_confidence_deviation (series, 4, 25, &value)
          && value == 0);
  double errors[] = { 1, 1 };
  assert (mulib_price_ensemble_blend (series, errors, 2, &value, &spread)
          && value == 15 && spread == 5);
  assert (!mulib_price_ensemble_blend (series, errors, 2, &value, &value));
  double x[] = { 1, 2, 3 }, y[] = { 9, 7, 5 };
  mulib_price_demand_model_t model;
  assert (mulib_price_demand_fit (x, y, 3, 0, &model));
  assert (fabs (model.slope + 2) < 1e-12 && model.r_squared == 1);
  assert (mulib_price_demand_predict (&model, 10) == 0);
  assert (!mulib_price_demand_fit (x, y, 3, -1, &model));
  mulib_price_causal_result_t result;
  double before[] = { 10, 20 }, after[] = { 15, 25 }, control[] = { 11, 21 };
  assert (
      mulib_price_causal_did (before, after, 2, before, control, 2, &result)
      && result.effect == 4 && result.standard_error == 0);
  assert (
      !mulib_price_causal_did (before, after, 1, before, control, 2, &result));
  assert (mulib_price_match_gtin ("4006381333931"));
  assert (!mulib_price_match_gtin ("4006381333932"));
  assert (mulib_price_match_score ("Widget-2", "widget 2", &value)
          && value == 1);
  assert (!mulib_price_match_score ("?!", "widget", &value));
}

int
main (void)
{
  money ();
  history ();
  statistics ();
  return 0;
}
