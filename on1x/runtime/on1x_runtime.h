#ifndef ON1X_RUNTIME_H
#define ON1X_RUNTIME_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <float.h>
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
#define ON1X_DEFER_ABI_VERSION 1
typedef struct on1x_defer_record on1x_defer_record;
typedef struct { on1x_defer_record *head; size_t count; } on1x_defer_stack;
void on1x_defer_push(on1x_defer_stack *,void (*)(on1x_env *),on1x_env *);
void on1x_defer_run(on1x_defer_stack *);
int64_t on1x_add_i64(int64_t a,int64_t b);
int64_t on1x_sub_i64(int64_t a,int64_t b);
int64_t on1x_mul_i64(int64_t a,int64_t b);
int64_t on1x_div_i64(int64_t a,int64_t b);
int64_t on1x_mod_i64(int64_t a,int64_t b);
int64_t on1x_neg_i64(int64_t a);
void on1x_print_i64(int64_t a);
void on1x_print_bool(bool a);
void on1x_print_string(const char *a);
#define ON1X_UNSIGNED_ABI_VERSION 1
#define ON1X_UNSIGNED_DECL(N,T) \
 T on1x_add_##N(T a,T b); T on1x_sub_##N(T a,T b); T on1x_mul_##N(T a,T b); \
 T on1x_div_##N(T a,T b); T on1x_mod_##N(T a,T b); void on1x_print_##N(T a); \
 T on1x_##N##_from_i64(int64_t a); int64_t on1x_##N##_to_i64(T a); \
 T on1x_##N##_from_bits(int64_t a); int64_t on1x_##N##_to_bits(T a); \
 T on1x_##N##_wrapping_add(T a,T b); T on1x_##N##_wrapping_sub(T a,T b); T on1x_##N##_wrapping_mul(T a,T b); \
 T on1x_##N##_and(T a,T b); T on1x_##N##_or(T a,T b); T on1x_##N##_xor(T a,T b); T on1x_##N##_not(T a); \
 T on1x_##N##_shl(T a,int64_t n); T on1x_##N##_shr(T a,int64_t n); \
 T on1x_##N##_from_u8(uint8_t a); T on1x_##N##_from_u16(uint16_t a); \
 T on1x_##N##_from_u32(uint32_t a); T on1x_##N##_from_u64(uint64_t a);
ON1X_UNSIGNED_DECL(u8,uint8_t)
ON1X_UNSIGNED_DECL(u16,uint16_t)
ON1X_UNSIGNED_DECL(u32,uint32_t)
ON1X_UNSIGNED_DECL(u64,uint64_t)
#undef ON1X_UNSIGNED_DECL
void on1x_assert(bool a);
#define ON1X_VALUE_DATA_ABI_VERSION 1
void on1x_bounds_index(int64_t index,int64_t length,const char *file,size_t line);
void on1x_bounds_slice(int64_t lo,int64_t hi,int64_t length,const char *file,size_t line);
void on1x_invalid_tag(const char *file,size_t line);
/* Initial binary64 target profile; other targets keep the existing runtime but
 * cannot compile generated f64 programs. This is not a general foreign ABI. */
#if defined(__linux__) && defined(__x86_64__) && defined(__SSE2__) && (defined(__GNUC__) || defined(__clang__)) && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021 && FLT_EVAL_METHOD == 0
#define ON1X_F64_ABI_VERSION 1
double on1x_add_f64(double a,double b);
double on1x_sub_f64(double a,double b);
double on1x_mul_f64(double a,double b);
double on1x_div_f64(double a,double b);
double on1x_neg_f64(double a);
bool on1x_eq_f64(double a,double b);
bool on1x_ne_f64(double a,double b);
bool on1x_lt_f64(double a,double b);
bool on1x_le_f64(double a,double b);
bool on1x_gt_f64(double a,double b);
bool on1x_ge_f64(double a,double b);
double on1x_f64_from_i64(int64_t a);
int64_t on1x_f64_to_i64(double a);
double on1x_f64_from_bits(int64_t bits);
int64_t on1x_f64_to_bits(double a);
bool on1x_f64_is_nan(double a);
bool on1x_f64_is_finite(double a);
bool on1x_f64_signbit(double a);
void on1x_print_f64(double a);
#if FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && FLT_MIN_EXP == -125
#define ON1X_F32_ABI_VERSION 1
float on1x_add_f32(float a,float b); float on1x_sub_f32(float a,float b);
float on1x_mul_f32(float a,float b); float on1x_div_f32(float a,float b); float on1x_neg_f32(float a);
bool on1x_eq_f32(float a,float b); bool on1x_ne_f32(float a,float b);
bool on1x_lt_f32(float a,float b); bool on1x_le_f32(float a,float b);
bool on1x_gt_f32(float a,float b); bool on1x_ge_f32(float a,float b);
float on1x_f32_from_i64(int64_t a); int64_t on1x_f32_to_i64(float a);
float on1x_f32_from_bits(int64_t bits); int64_t on1x_f32_to_bits(float a);
float on1x_f32_from_f64(double a); double on1x_f64_from_f32(float a);
bool on1x_f32_is_nan(float a); bool on1x_f32_is_finite(float a); bool on1x_f32_signbit(float a);
void on1x_print_f32(float a);
#define ON1X_UFLOAT_DECL(N,T) \
 T on1x_##N##_from_f32(float a); T on1x_##N##_from_f64(double a); \
 float on1x_f32_from_##N(T a); double on1x_f64_from_##N(T a);
ON1X_UFLOAT_DECL(u8,uint8_t)
ON1X_UFLOAT_DECL(u16,uint16_t)
ON1X_UFLOAT_DECL(u32,uint32_t)
ON1X_UFLOAT_DECL(u64,uint64_t)
#undef ON1X_UFLOAT_DECL
#endif
#endif
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
