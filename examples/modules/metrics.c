#define MULIB_IMPL_METRICS
#include "mulib-metrics.h"
#include <assert.h>
int
main (void)
{
  mulib_metrics_t metrics = { 0 };
  assert (mulib_metrics_record (&metrics, 200, 100, 200, 500));
  assert (metrics.requests == 1 && metrics.status_class[2] == 1
          && metrics.duration_us == 500);
  return 0;
}
