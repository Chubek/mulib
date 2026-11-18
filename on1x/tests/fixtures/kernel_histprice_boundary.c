/* An independent C caller oracle, not an On1x binding or admission proof. */
#include "mulib-histprice.h"
#include <math.h>

typedef int holt_fn(const double *, size_t, double, double, double, size_t,
                    mulib_histprice_forecast_t *);
_Static_assert(_Generic(&mulib_histprice_holt, holt_fn *: 1, default: 0),
               "Holt declaration changed");

int main(void)
{
    const double samples[] = {10.0, 12.0, 14.0};
    mulib_histprice_forecast_t out = {17.0, 23.0};
    if (mulib_histprice_holt(samples, 3, 0.5, 0.5, 0.0, 2, &out) ||
        out.point != 17.0 || out.variance != 23.0)
        return 1;
    if (!mulib_histprice_holt(samples, 3, 0.5, 0.5, 1.0, 2, &out) ||
        out.point != 18.0 || !isnan(out.variance))
        return 2;
    return 0;
}
