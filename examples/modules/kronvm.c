#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_DATE
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_EVTSCHED
#define MULIB_IMPL_KRONVM
#define MULIB_IMPL_KVDB
#define MULIB_IMPL_VARINT
#include "mulib-kronvm.h"

/* Drive a private VM using a simulated clock. */
int
main (void)
{
  char directory[] = "/tmp/kronvm-example-XXXXXX";
  char cache[sizeof directory + sizeof "/events.kvdb"];
  mulib_kronvm_t *vm = NULL;
  mulib_kronvm_result_t result;
  uint64_t id;
  if (!mkdtemp (directory))
    return 1;
  int ok = mulib_kronvm_open (directory, &vm) == 0
           && mulib_kronvm_delegate (
                  vm, "DELEGATE `echo foobar` ONCE AT <10>;", 0, &id)
                  >= 0
           && mulib_kronvm_dispatch (vm, 10, &result) > 0 && result.id == id
           && result.launched && !result.execution_errno
           && result.wait_status == 0;
  mulib_kronvm_close (vm);
  snprintf (cache, sizeof cache, "%s/events.kvdb", directory);
  unlink (cache);
  rmdir (directory);
  return !ok;
}
