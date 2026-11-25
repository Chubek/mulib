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
#include <assert.h>

int
main (void)
{
  char dir[] = "/tmp/kronvm-test-XXXXXX", path[100], backup[100];
  mulib_kronvm_t *vm, *other = NULL;
  mulib_kronvm_result_t r;
  uint64_t a, b, c;
  int64_t deadline;
  assert (mkdtemp (dir));
  snprintf (path, sizeof path, "%s/events.kvdb", dir);
  snprintf (backup, sizeof backup, "%s/backup", dir);
  assert (!mulib_kronvm_open (dir, &vm));
  assert (mulib_kronvm_open (dir, &other) < 0 && !other);
  const char *invalid[] = { "",
                            "DELEGATE `x` EVERY <0s>;",
                            "EXECUTE \"relative\" ONCE AT <9>;",
                            "DELEGATE `x` ONCE AT <12:30:00; Feb 30>;",
                            "DELEGATE `x` EVERY <1h> FOR <1s>;",
                            "DELEGATE `x` EVERY <9223372036854775808s>;",
                            "DELEGATE `x` EVERY <1s>; trailing",
                            "DELEGATE `x` ONCEAT <0>;",
                            "DELEGATE `x` EVERY <1s> UNTIL SIGNAL(0);" };
  for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i)
    {
      assert (mulib_kronvm_delegate (vm, invalid[i], 0, &a) < 0
              && errno == EINVAL);
    }
  assert (!mulib_kronvm_next (vm, &deadline));
  assert (!mulib_kronvm_delegate (
      vm, "DELEGATE `exit 17` ONCE AT <12:30:00; Jan 24>;", 0, &a));
  assert (mulib_kronvm_next (vm, &deadline) == 1
          && deadline == 23 * 86400 + 45000);
  assert (!mulib_kronvm_pull (vm, a) && mulib_kronvm_pull (vm, a) < 0
          && errno == ENOENT);
  /* Admission after the 2024 leap day advances to 2028-02-29 UTC. */
  assert (!mulib_kronvm_delegate (
      vm, "EXECUTE \"/bin/true\" ONCE AT <00:00:00; Feb 29>;", 1709251200,
      &a));
  assert (mulib_kronvm_next (vm, &deadline) == 1 && deadline == 1835395200);
  assert (!mulib_kronvm_pull (vm, a));
  assert (!mulib_kronvm_delegate (
      vm, "DELEGATE `exit 17` EVERY <2s> UNTIL SIGNAL(9);", 0, &a));
  assert (!mulib_kronvm_delegate (vm, "EXECUTE \"/bin/true\" ONCE AT <8>;", 0,
                                  &b));
  assert (!mulib_kronvm_dispatch (vm, 1, &r));
  assert (!rename (path, backup));
  assert (mulib_kronvm_dispatch (vm, 7, &r) < 0);
  assert (mulib_kronvm_next (vm, &deadline) == 1 && deadline == 2);
  assert (!rename (backup, path));
  assert (mulib_kronvm_dispatch (vm, 7, &r) == 1 && r.id == a
          && r.deadline == 2 && WEXITSTATUS (r.wait_status) == 17);
  mulib_kronvm_close (vm);
  assert (!mulib_kronvm_open (dir, &vm));
  assert (mulib_kronvm_dispatch (vm, 8, &r) == 1 && r.id == b && r.launched
          && !r.wait_status);
  assert (mulib_kronvm_dispatch (vm, 8, &r) == 1 && r.id == a);
  assert (!mulib_kronvm_delegate (
      vm, "EXECUTE \"/bin/true\" EVERY <1s> UNTIL SIGNAL(9);", 8, &b));
  assert (!mulib_kronvm_delegate (
      vm, "EXECUTE \"/bin/true\" EVERY <1s> FOR <2s>;", 8, &c));
  assert (!mulib_kronvm_signal (vm, 9));
  assert (mulib_kronvm_dispatch (vm, 9, &r) == 1 && r.id == c && r.launched);
  assert (mulib_kronvm_dispatch (vm, 11, &r) == 1 && r.id == c && !r.launched
          && !r.execution_errno);
  assert (!mulib_kronvm_next (vm, &deadline));
  assert (!mulib_kronvm_delegate (
      vm, "EXECUTE \"/nonexistent-kronvm-test\" EVERY <1s>;", INT64_MAX - 1,
      &a));
  assert (mulib_kronvm_dispatch (vm, INT64_MAX, &r) == 1
          && WEXITSTATUS (r.wait_status) == 127);
  assert (!mulib_kronvm_next (vm, &deadline));
  mulib_kronvm_close (vm);
  assert (!mulib_kronvm_open (dir, &vm));
  assert (!mulib_kronvm_next (vm, &deadline));
  assert (!mulib_kronvm_delegate (
      vm, "EXECUTE \"/bin/true\" ONCE AT <0> PERSISTENT;", 0, &b));
  assert (b > a);
  mulib_kronvm_close (vm);
  FILE *f = fopen (path, "wb");
  assert (f);
  fputs ("bad", f);
  assert (!fclose (f));
  assert (mulib_kronvm_open (dir, &other) < 0 && !other);
  assert (!unlink (path) && !rmdir (dir));
}
