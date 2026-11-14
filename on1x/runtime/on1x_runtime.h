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
#define ON1X_DATA_ABI_VERSION 1
typedef struct on1x_sexpr on1x_sexpr;
typedef struct on1x_buffer on1x_buffer;
/* Managed parameters borrow; managed results own. Nil is NULL. Text snapshots
 * are NUL-free, bounded, process-lifetime strings, reclaimed at normal exit. */
void on1x_data_retain(void *value);
void on1x_data_release(void *value);
on1x_sexpr *on1x_sx_nil(void);
on1x_sexpr *on1x_sx_symbol(const char *text);
on1x_sexpr *on1x_sx_string(const char *text);
on1x_sexpr *on1x_sx_cons(on1x_sexpr *head,on1x_sexpr *tail);
on1x_sexpr *on1x_sx_head(on1x_sexpr *value);
on1x_sexpr *on1x_sx_tail(on1x_sexpr *value);
int64_t on1x_sx_kind(on1x_sexpr *value);
const char *on1x_sx_text(on1x_sexpr *value);
int64_t on1x_data_live_nodes(void);
int64_t on1x_text_len(const char *text);
int64_t on1x_text_byte(const char *text,int64_t index);
const char *on1x_text_slice(const char *text,int64_t begin,int64_t end);
const char *on1x_text_concat(const char *left,const char *right);
on1x_buffer *on1x_buffer_new(void);
void on1x_buffer_append(on1x_buffer *buffer,const char *text);
void on1x_buffer_byte(on1x_buffer *buffer,int64_t byte);
const char *on1x_buffer_text(on1x_buffer *buffer);
int64_t on1x_buffer_len(on1x_buffer *buffer);
void on1x_buffer_clear(on1x_buffer *buffer);
const char *on1x_io_stdin(void);
void on1x_io_stdout(const char *text);
void on1x_io_stderr(const char *text);
const char *on1x_tool_env(const char *name);
const char *on1x_tool_read_file(const char *path);
void on1x_tool_fail(const char *message);
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
