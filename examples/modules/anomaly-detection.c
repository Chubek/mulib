#define MULIB_IMPL_STATS
#define MULIB_IMPL_ANOMALY_DETECTION
#include "mulib-anomaly-detection.h"
#include <assert.h>
int main(void) {
  mulib_stats_t baseline; mulib_stats_init(&baseline);
  assert(mulib_anomaly_train(&baseline, 1) && mulib_anomaly_train(&baseline, 2));
  assert(mulib_anomaly_detect(&baseline, 100, 3) == 1); return 0;
}
