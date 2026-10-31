#include "../container-stack-common.h"
int main (void)
{
  mulib_container_log log;
  const mulib_container_log_record *record;
  uint64_t cursor = 0;
  assert (!mulib_container_log_init (&log, 16, 4096, 256, 1));
  assert (!mulib_container_log_append (&log, "worker", MULIB_CONTAINER_LOG_INFO,
      1, 100, "ready\n", 6, NULL));
  assert (!mulib_container_log_next (&log, &cursor, "worker", MULIB_CONTAINER_LOG_INFO, &record));
  assert (record->message.length == 6 && cursor == 2);
  mulib_container_log_free (&log);
  return 0;
}
