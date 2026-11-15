#ifndef ON1X_BASIS_NATIVE_H
#define ON1X_BASIS_NATIVE_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact `i64`, `bool`, and `string` ABIs used by On1x raw externs.
   These are experimental adapters, not a verified safe FFI. */
int64_t basis_native_arr_new(int64_t length, int64_t initial);
int64_t basis_native_arr_len(int64_t handle);
int64_t basis_native_arr_get(int64_t handle, int64_t index);
void basis_native_arr_set(int64_t handle, int64_t index, int64_t value);
int64_t basis_native_arr_copy(int64_t handle);
void basis_native_arr_free(int64_t handle);
bool basis_native_write_text(const char *path, const char *content);
bool basis_native_append_text(const char *path, const char *content);
const char *basis_native_read_hex(const char *path);
bool basis_native_write_hex(const char *path, const char *hex_text);
bool basis_native_fs_exists(const char *path);
bool basis_native_fs_isdir(const char *path);
int64_t basis_native_fs_size(const char *path);
bool basis_native_fs_remove(const char *path);
bool basis_native_fs_mkdir(const char *path);
bool basis_native_fs_rename(const char *old_path, const char *new_path);
bool basis_native_fs_chdir(const char *path);
const char *basis_native_fs_cwd(void);
void basis_native_process_exit(int64_t code);
int64_t basis_native_time_unix_seconds(void);
int64_t basis_native_time_monotonic_ns(void);
const char *basis_native_date_utc(int64_t unix_seconds);

const char *basis_native_real_add(const char *, const char *);
const char *basis_native_real_sub(const char *, const char *);
const char *basis_native_real_mul(const char *, const char *);
const char *basis_native_real_div(const char *, const char *);
int64_t basis_native_real_compare(const char *, const char *);
const char *basis_native_real_neg(const char *);
const char *basis_native_real_abs(const char *);
const char *basis_native_math_sqrt(const char *);
const char *basis_native_math_exp(const char *);
const char *basis_native_math_log(const char *);
const char *basis_native_math_sin(const char *);
const char *basis_native_math_cos(const char *);
const char *basis_native_math_pow(const char *,const char *);
bool basis_native_ieee_isfinite(const char *);
#ifdef __cplusplus
}
#endif
#endif
