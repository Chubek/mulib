#include "../runtime/basis_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void) {
  int64_t a=basis_native_arr_new(3,-7);
  assert(a>0 && basis_native_arr_len(a)==3);
  assert(basis_native_arr_get(a,1)==-7);
  basis_native_arr_set(a,1,123);
  assert(basis_native_arr_get(a,1)==123);
  int64_t b=basis_native_arr_copy(a);
  basis_native_arr_set(b,1,45);
  assert(basis_native_arr_get(a,1)==123);
  assert(basis_native_arr_get(b,1)==45);
  basis_native_arr_free(a);basis_native_arr_free(b);
  assert(basis_native_arr_len(basis_native_arr_new(0,33))==0);
  const char *path="/tmp/on1x-basis-native-test.bin";
  assert(basis_native_write_hex(path,"00ff010a"));
  assert(strcmp(basis_native_read_hex(path),"00ff010a")==0);
  assert(basis_native_fs_exists(path));
  assert(basis_native_fs_size(path)==4);
  assert(!basis_native_write_hex(path,"xyz"));
  assert(basis_native_write_text(path,"Hello"));
  assert(basis_native_append_text(path," World"));
  assert(strcmp(basis_native_read_hex(path),"48656c6c6f20576f726c64")==0);
  assert(basis_native_fs_remove(path));
  assert(!basis_native_fs_exists(path));
  assert(strlen(basis_native_fs_cwd())>0);
  assert(basis_native_time_unix_seconds()>0);
  assert(basis_native_time_monotonic_ns()>0);
  assert(strlen(basis_native_date_utc(0))==20);
  puts("basis native tests passed");
  return 0;
}
