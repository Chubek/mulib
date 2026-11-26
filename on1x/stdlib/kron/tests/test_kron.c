#define _POSIX_C_SOURCE 200809L
#include "runtime.h"
#include "mulib-endian.h"
#include "mulib-kronvm.h"
#include "mulib-kvdb.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef BASIS_KRON_TEST_WRAP
static int fail_directory_sync, fail_file_sync, fail_alloc = -1;
int __real_fsync(int fd);
void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t width);
void *__real_realloc(void *p, size_t n);

int __wrap_fsync(int fd)
{
  if (fail_directory_sync || fail_file_sync) {
    struct stat st;
    assert(fstat(fd, &st) == 0);
    int *fail = S_ISDIR(st.st_mode) ? &fail_directory_sync : &fail_file_sync;
    if (*fail) {
      *fail = 0;
      errno = EIO;
      return -1;
    }
  }
  return __real_fsync(fd);
}

static int allocation_fails(void)
{
  if (fail_alloc < 0 || fail_alloc-- > 0) return 0;
  errno = ENOMEM;
  return 1;
}

void *__wrap_malloc(size_t n)
{
  return allocation_fails() ? NULL : __real_malloc(n);
}
void *__wrap_calloc(size_t n, size_t width)
{
  return allocation_fails() ? NULL : __real_calloc(n, width);
}
void *__wrap_realloc(void *p, size_t n)
{
  return allocation_fails() ? NULL : __real_realloc(p, n);
}
#endif

static const unsigned char descriptor[] = {'J', 0, 255, '1'};
static const basis_kron_schedule immediate = {
  BASIS_KRON_ONCE, 0, BASIS_KRON_PERSISTENT, 0
};

struct state {
  basis_kron_vm *vm;
  unsigned authorizations, runs;
  int permission, status;
  uint64_t id;
  int64_t due;
};

static int authorize(void *opaque, const basis_kron_occurrence *job)
{
  struct state *s = opaque;
  ++s->authorizations;
  assert(job->size == sizeof descriptor);
  assert(memcmp(job->data, descriptor, job->size) == 0);
  assert(s->id == job->id && s->due == job->deadline);
  int64_t unused;
  /* A callback cannot mutate, inspect or close the VM reentrantly. */
  assert(basis_kron_next(s->vm, &unused) == BASIS_KRON_NEXT_ERROR &&
         errno == EBUSY);
  assert(basis_kron_pull(s->vm, job->id) == BASIS_KRON_ERROR && errno == EBUSY);
  basis_kron_close(s->vm);
  assert(errno == EBUSY);
  return s->permission;
}

static int run(void *opaque, const basis_kron_occurrence *job)
{
  struct state *s = opaque;
  assert(s->permission == 1);
  assert(job->size == sizeof descriptor);
  assert(memcmp(job->data, descriptor, job->size) == 0);
  assert(s->id == job->id && s->due == job->deadline);
  ++s->runs;
  return s->status;
}

static void validation(basis_kron_vm *vm)
{
  static const basis_kron_schedule invalid[] = {
    {BASIS_KRON_EVERY, 0, BASIS_KRON_PERSISTENT, 0},
    {BASIS_KRON_ONCE, -1, BASIS_KRON_PERSISTENT, 0},
    {BASIS_KRON_EVERY, 1, BASIS_KRON_PERSISTENT, 4},
    {BASIS_KRON_ONCE, 3, BASIS_KRON_FOR, 1},
    {BASIS_KRON_ONCE, 0, BASIS_KRON_FOR, 0},
    {BASIS_KRON_ONCE, 0, BASIS_KRON_UNTIL_SIGNAL, 0},
    {BASIS_KRON_ONCE, 0, BASIS_KRON_UNTIL_SIGNAL, INT64_MAX},
    {(basis_kron_cadence)99, 0, BASIS_KRON_PERSISTENT, 0},
    {BASIS_KRON_ONCE, 0, (basis_kron_lifetime)99, 0},
  };
  basis_kron_schedule schedule = {BASIS_KRON_EVERY, INT64_MAX, BASIS_KRON_PERSISTENT, 0};
  assert(basis_kron_validate_schedule(&schedule, 1) == -1 && errno == EOVERFLOW);
  schedule = (basis_kron_schedule){BASIS_KRON_ONCE, 0, BASIS_KRON_FOR, INT64_MAX};
  assert(basis_kron_validate_schedule(&schedule, 1) == -1 && errno == EOVERFLOW);
  assert(basis_kron_validate_schedule(&schedule, 0) == 0);
  for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
    uint64_t id = 42;
    assert(basis_kron_validate_schedule(&invalid[i], 0) == -1 && errno == EINVAL);
    assert(basis_kron_delegate(vm, &invalid[i], 0, descriptor, sizeof descriptor,
                               &id) == BASIS_KRON_ERROR && errno == EINVAL);
    assert(id == 42);
  }
  uint64_t id = 9;
  assert(basis_kron_delegate(vm, &immediate, -1, descriptor, sizeof descriptor,
                             &id) == BASIS_KRON_ERROR && errno == EINVAL);
  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, 0,
                             &id) == BASIS_KRON_ERROR && errno == EINVAL);
  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, BASIS_KRON_MAX_JOB_BYTES + 1,
                             &id) == BASIS_KRON_ERROR && errno == E2BIG);
  assert(id == 9);
  assert(basis_kron_signal(vm, 0) == BASIS_KRON_ERROR && errno == EINVAL);
  assert(basis_kron_pull(vm, 9) == BASIS_KRON_ERROR && errno == ENOENT);
  assert(basis_kron_pull(vm, 0) == BASIS_KRON_ERROR && errno == EINVAL);

  unsigned char *large = calloc(BASIS_KRON_MAX_JOB_BYTES, 1);
  assert(large);
  assert(basis_kron_delegate(vm, &immediate, 0, large, BASIS_KRON_MAX_JOB_BYTES,
                             &id) == BASIS_KRON_COMMITTED);
  free(large);
  assert(basis_kron_pull(vm, id) == BASIS_KRON_COMMITTED);
}

static void corrupt_job_record(const char *path, uint64_t ticket)
{
  mulib_kvdb_t *db;
  unsigned char key[8], malformed[24] = {0};
  mulib_endian_store_be64(key, ticket);
  mulib_endian_store_be64(malformed, ticket);
  assert(mulib_kvdb_open(path, 0, &db) == MULIB_KVDB_OK);
  assert(mulib_kvdb_set(db, key, sizeof key, malformed, sizeof malformed) == MULIB_KVDB_OK);
  assert(mulib_kvdb_commit(db) == MULIB_KVDB_OK);
  mulib_kvdb_close(db);
}

static void check_snapshot_privacy(const char *dir, const char *path,
                                   const char *backup)
{
  struct stat st;
  basis_kron_vm *vm = NULL;
  assert(lstat(path, &st) == 0 && S_ISREG(st.st_mode) &&
         st.st_uid == geteuid() && !(st.st_mode & (S_IRWXG | S_IRWXO)));
  assert(chmod(path, 0640) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EACCES && !vm);
  assert(chmod(path, 0604) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EACCES && !vm);
  assert(chmod(path, 0600) == 0);

  assert(rename(path, backup) == 0);
  assert(symlink(backup, path) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == ELOOP && !vm);
  assert(unlink(path) == 0);
  assert(mkfifo(path, 0600) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EINVAL && !vm);
  assert(unlink(path) == 0);
  assert(mkdir(path, 0700) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EINVAL && !vm);
  assert(rmdir(path) == 0);
  assert(rename(backup, path) == 0);
}

int main(void)
{
  char dir[] = "/tmp/std-kron-jobs-XXXXXX", legacy[] = "/tmp/std-kron-old-XXXXXX";
  char path[96], backup[96], alias_slash[100], legacy_path[96];
  assert(mkdtemp(dir) && mkdtemp(legacy));
  assert(snprintf(path, sizeof path, "%s/events.kvdb", dir) > 0);
  assert(snprintf(backup, sizeof backup, "%s/backup", dir) > 0);
  assert(snprintf(legacy_path, sizeof legacy_path, "%s/events.kvdb", legacy) > 0);

  basis_kron_vm *vm = NULL, *other = NULL;
  assert(basis_kron_open(NULL, &vm) == BASIS_KRON_ERROR && errno == EINVAL && !vm);
  assert(basis_kron_open("relative", &vm) == BASIS_KRON_ERROR && errno == EINVAL && !vm);
  assert(basis_kron_open(backup, &vm) == BASIS_KRON_ERROR && errno == ENOENT && !vm);
  assert(chmod(dir, 0770) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EACCES && !vm);
  assert(chmod(dir, 0740) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EACCES && !vm);
  assert(chmod(dir, 0701) == 0);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == EACCES && !vm);
  assert(chmod(dir, 0700) == 0);
  assert(symlink(dir, backup) == 0);
  assert(basis_kron_open(backup, &vm) == BASIS_KRON_ERROR && errno == ELOOP && !vm);
  assert(snprintf(alias_slash, sizeof alias_slash, "%s///", backup) > 0);
  assert(basis_kron_open(alias_slash, &vm) == BASIS_KRON_ERROR && errno == ELOOP && !vm);
  assert(unlink(backup) == 0);
#ifdef BASIS_KRON_TEST_WRAP
  fail_alloc = 1;
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_ERROR && errno == ENOMEM && !vm);
  fail_directory_sync = 1;
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_UNCERTAIN && vm &&
         !fail_directory_sync);
#else
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED && vm);
#endif
  assert(basis_kron_open(dir, &other) == BASIS_KRON_ERROR && !other);
  validation(vm);

  /* The kernel itself rejects mismatched entrypoints, even if bypassing
   * runtime.h. An old executable's cache remains untouched by std.kron. */
  mulib_kronvm_t *old = NULL, *direct = NULL;
  mulib_kronvm_result_t old_result;
  uint64_t a, b, c;
  assert(chmod(legacy, 0755) == 0);
  assert(mulib_kronvm_open(legacy, &old) == 0);
  assert(mulib_kronvm_delegate(old, "DELEGATE `exit 17` ONCE AT <0>;", 0, &a) == 0);
  assert(chmod(legacy_path, 0644) == 0);
  mulib_kronvm_close(old);
  assert(mulib_kronvm_open(legacy, &old) == 0);
  mulib_kronvm_close(old);
  assert(chmod(legacy, 0700) == 0 && chmod(legacy_path, 0600) == 0);
  assert(basis_kron_open(legacy, &other) == BASIS_KRON_ERROR &&
         errno == EINVAL && !other);
  basis_kron_close(vm);
  check_snapshot_privacy(dir, path, backup);
  assert(mulib_kronvm_open(dir, &direct) < 0 && errno == EINVAL && !direct);
  assert(mulib_kronvm_open_jobs(dir, &direct) == 0);
  assert(mulib_kronvm_delegate(direct, "DELEGATE `exit 17` ONCE AT <0>;", 0, &a) < 0 &&
         errno == EINVAL);
  assert(mulib_kronvm_dispatch(direct, 0, &old_result) < 0 && errno == EINVAL);
  mulib_kronvm_close(direct);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED);

  struct state s = {.vm = vm, .permission = 1};
  basis_kron_host host = {&s, authorize, run}, incomplete = {&s, authorize, NULL};
  basis_kron_result result = {.id = 123, .deadline = -1, .run_status = 123};
  int64_t deadline = -1;
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY && deadline == -1);

  basis_kron_schedule every = {BASIS_KRON_EVERY, 2, BASIS_KRON_UNTIL_SIGNAL, 9};
  basis_kron_schedule once = {BASIS_KRON_ONCE, 8, BASIS_KRON_PERSISTENT, 0};
  unsigned char input[sizeof descriptor];
  memcpy(input, descriptor, sizeof input);
  assert(basis_kron_delegate(vm, &every, 0, input, sizeof input, &a) == BASIS_KRON_COMMITTED);
  memset(input, 0, sizeof input); /* caller may release input immediately */
  assert(basis_kron_delegate(vm, &once, 0, descriptor, sizeof descriptor,
                             &b) == BASIS_KRON_COMMITTED && b > a);
  basis_kron_close(vm);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED);
  s.vm = vm;
  assert(basis_kron_dispatch(vm, -1, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == EINVAL);
  assert(basis_kron_dispatch(vm, 2, &incomplete, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == EINVAL && !s.authorizations);
  assert(basis_kron_dispatch(vm, 1, &host, &result) == BASIS_KRON_IDLE);
  assert(result.id == 123 && result.deadline == -1 && result.run_status == 123);
  assert(chmod(dir, 0755) == 0);
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == EACCES && result.id == 123 && !s.authorizations);
  assert(chmod(dir, 0700) == 0);
  assert(chmod(path, 0644) == 0);
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == EACCES && result.id == 123 && !s.authorizations);
  assert(chmod(path, 0600) == 0);
  assert(rename(path, backup) == 0);
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_DISPATCH_ERROR);
  assert(symlink(backup, path) == 0);
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == ELOOP);
  assert(unlink(path) == 0);
  assert(mkfifo(path, 0600) == 0);
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         errno == EINVAL);
  assert(unlink(path) == 0);
  assert(!s.authorizations && !s.runs && result.id == 123);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 2);
  assert(rename(backup, path) == 0);

  /* Revoke approval after admission and reopening. A non-1 decision fails closed. */
  s.permission = 2;
  s.id = a;
  s.due = 2;
  assert(basis_kron_dispatch(vm, 7, &host, &result) == BASIS_KRON_CONSUMED &&
         result.disposition == BASIS_KRON_DENIED && result.id == a && result.deadline == 2);
  assert(s.authorizations == 1 && s.runs == 0);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 8);
  basis_kron_close(vm);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED);
  s.vm = vm;
  s.permission = 1;
  s.id = b;
  s.due = 8;
  /* Tie order survives reopen: b was enqueued before a rearmed for time 8. */
  assert(basis_kron_dispatch(vm, 8, &host, &result) == BASIS_KRON_CONSUMED &&
         result.disposition == BASIS_KRON_RAN && result.run_status == 0 &&
         result.id == b && result.deadline == 8);
  s.id = a;
  s.status = 37;
  assert(basis_kron_dispatch(vm, 8, &host, &result) == BASIS_KRON_CONSUMED &&
         result.disposition == BASIS_KRON_RUN_FAILED && result.run_status == 37 &&
         result.id == a && result.deadline == 8);
  assert(s.authorizations == 3 && s.runs == 2);

  assert(basis_kron_delegate(vm, &every, 8, descriptor, sizeof descriptor,
                             &b) == BASIS_KRON_COMMITTED);
  assert(basis_kron_signal(vm, 9) == BASIS_KRON_COMMITTED);
  assert(basis_kron_pull(vm, a) == BASIS_KRON_ERROR && errno == ENOENT);
  assert(basis_kron_pull(vm, b) == BASIS_KRON_ERROR && errno == ENOENT);
  const basis_kron_schedule finite = {BASIS_KRON_EVERY, 1, BASIS_KRON_FOR, 2};
  assert(basis_kron_delegate(vm, &finite, 8, descriptor, sizeof descriptor,
                             &c) == BASIS_KRON_COMMITTED);
  basis_kron_close(vm);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED);
  s.vm = vm;
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 9);
  assert(basis_kron_dispatch(vm, 11, &host, &result) == BASIS_KRON_CONSUMED &&
         result.disposition == BASIS_KRON_EXPIRED && result.id == c && result.deadline == 9);
  assert(s.authorizations == 3 && s.runs == 2);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);

  const basis_kron_schedule extreme = {BASIS_KRON_EVERY, 1, BASIS_KRON_PERSISTENT, 0};
  assert(basis_kron_delegate(vm, &extreme, INT64_MAX - 1, descriptor, sizeof descriptor,
                             &a) == BASIS_KRON_COMMITTED && a > c);
  s.id = a;
  s.due = INT64_MAX;
  s.status = 0;
  assert(basis_kron_dispatch(vm, INT64_MAX, &host, &result) == BASIS_KRON_CONSUMED &&
         result.disposition == BASIS_KRON_RAN && result.deadline == INT64_MAX);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);

#ifdef BASIS_KRON_TEST_WRAP
  c = 555;
  for (int i = 0; i < 3; ++i) {
    fail_alloc = i;
    assert(basis_kron_delegate(vm, &immediate, 0, descriptor, sizeof descriptor,
                               &c) == BASIS_KRON_ERROR && errno == ENOMEM);
    assert(c == 555 && basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);
  }
  fail_file_sync = 1;
  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, sizeof descriptor, &c)
         == BASIS_KRON_ERROR && errno == EIO && c == 555 && !fail_file_sync);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);
  fail_directory_sync = 1;
  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, sizeof descriptor, &c)
         == BASIS_KRON_UNCERTAIN && c > a && !fail_directory_sync);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 0);
  fail_file_sync = 1;
  assert(basis_kron_dispatch(vm, 0, &host, &result) == BASIS_KRON_DISPATCH_ERROR &&
         result.id == a && !fail_file_sync);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 0);
  fail_directory_sync = 1;
  assert(basis_kron_dispatch(vm, 0, &host, &result) == BASIS_KRON_CONSUMED_UNCERTAIN &&
         result.disposition == BASIS_KRON_NOT_RUN_UNCERTAIN && result.id == c &&
         result.deadline == 0 && !fail_directory_sync);
  assert(s.authorizations == 4 && s.runs == 3);
  basis_kron_close(vm);
  assert(basis_kron_open(dir, &vm) == BASIS_KRON_COMMITTED);
  s.vm = vm;
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);

  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, sizeof descriptor, &c)
         == BASIS_KRON_COMMITTED);
  fail_directory_sync = 1;
  assert(basis_kron_pull(vm, c) == BASIS_KRON_UNCERTAIN && !fail_directory_sync);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);
  assert(basis_kron_delegate(vm, &every, 0, descriptor, sizeof descriptor, &c)
         == BASIS_KRON_COMMITTED);
  fail_directory_sync = 1;
  assert(basis_kron_signal(vm, 9) == BASIS_KRON_UNCERTAIN && !fail_directory_sync);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);
#endif

  assert(basis_kron_delegate(vm, &immediate, 0, descriptor, sizeof descriptor,
                             &c) == BASIS_KRON_COMMITTED && c > a);
  basis_kron_close(vm);
  corrupt_job_record(path, c);
  assert(basis_kron_open(dir, &other) == BASIS_KRON_ERROR &&
         errno == EINVAL && !other);
  /* Cleanups also check that no transaction leaked a temporary sibling. */
  assert(unlink(path) == 0 && rmdir(dir) == 0);
  assert(unlink(legacy_path) == 0 && rmdir(legacy) == 0);
  return 0;
}
