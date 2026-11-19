#ifndef ON1X_RT_TEST_FIXTURE_MODULE_H
#define ON1X_RT_TEST_FIXTURE_MODULE_H

#include "On1xRT.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xRT_TestFixtureCounters {
  unsigned initialize_calls;
  unsigned shutdown_calls;
  unsigned instance_create_calls;
  unsigned instance_destroy_calls;
  unsigned function_calls;
} On1xRT_TestFixtureCounters;

ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL(void);

void on1x_rt_test_fixture_reset(void);
On1xRT_TestFixtureCounters on1x_rt_test_fixture_counters(void);
void on1x_rt_test_fixture_set_engine(On1xRT_Engine *engine);

#ifdef __cplusplus
}
#endif

#endif
