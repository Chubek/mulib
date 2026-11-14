#ifndef ON1X_RUNTIME_H
#define ON1X_RUNTIME_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_json on1x_json;
/* Internal generated-code ABI, not a foreign callback/extension interface.
 * Function arguments borrow environments; function results own a reference.
 * Contexts are single-threaded. destroy releases fields, never the header. */
#define ON1X_CLOSURE_ABI_VERSION 1
typedef struct on1x_env on1x_env;
struct on1x_env {
  size_t references;
  void (*destroy)(on1x_env *);
  on1x_env *next;
};
on1x_env *on1x_env_alloc(size_t size, void (*destroy)(on1x_env *));
void on1x_env_retain(on1x_env *environment);
void on1x_env_release(on1x_env *environment);
int64_t on1x_add_i64(int64_t a,int64_t b);
int64_t on1x_sub_i64(int64_t a,int64_t b);
int64_t on1x_mul_i64(int64_t a,int64_t b);
int64_t on1x_div_i64(int64_t a,int64_t b);
int64_t on1x_mod_i64(int64_t a,int64_t b);
int64_t on1x_neg_i64(int64_t a);
void on1x_print_i64(int64_t a);
void on1x_print_bool(bool a);
void on1x_print_string(const char *a);
void on1x_assert(bool a);
on1x_json *on1x_json_parse_or_die(const char *text);
const on1x_json *on1x_json_get_or_die(const on1x_json *json,const char *key);
int64_t on1x_json_size(const on1x_json *json);
int64_t on1x_json_number(const on1x_json *json);
void on1x_json_print(const on1x_json *json);
void on1x_json_drop(on1x_json *json);
#ifdef __cplusplus
}
#endif
#endif
