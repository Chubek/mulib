/* Compile-only ABI fixture extracted from MuLIB's public Mune declarations.
 * Test intentionally does not implement Lua behavior. Do not ship or install
 * as a replacement for the real mulib-mune.h. */
#ifndef MULIB_MUNE_H
#define MULIB_MUNE_H
#include <stddef.h>
#include <stdint.h>
typedef struct mulib_mune_state mulib_mune_state_t;
typedef struct mulib_mune_result mulib_mune_result_t;
typedef struct mulib_mune_object mulib_mune_object_t;
typedef enum mulib_mune_status {MULIB_MUNE_OK,MULIB_MUNE_INVALID,MULIB_MUNE_SYNTAX,MULIB_MUNE_NAME,MULIB_MUNE_ARITHMETIC,MULIB_MUNE_LIMIT,MULIB_MUNE_MEMORY,MULIB_MUNE_IO,MULIB_MUNE_SEMANTIC,MULIB_MUNE_RUNTIME} mulib_mune_status_t;
typedef struct mulib_mune_error {mulib_mune_status_t code;size_t offset;const char *message;}mulib_mune_error_t;
typedef enum mulib_mune_type {MULIB_MUNE_NIL,MULIB_MUNE_BOOLEAN,MULIB_MUNE_INTEGER,MULIB_MUNE_NUMBER,MULIB_MUNE_STRING,MULIB_MUNE_TABLE,MULIB_MUNE_FUNCTION,MULIB_MUNE_USERDATA,MULIB_MUNE_THREAD}mulib_mune_type_t;
typedef struct mulib_mune_value {mulib_mune_type_t type;union {int boolean;int64_t integer;double number;mulib_mune_object_t *object;}as;}mulib_mune_value_t;
typedef struct mulib_mune_runtime_options {size_t memory_bytes,steps,call_depth,results;}mulib_mune_runtime_options_t;
typedef mulib_mune_status_t (*mulib_mune_native_fn)(mulib_mune_state_t*,const mulib_mune_value_t*,size_t,mulib_mune_result_t*,void*);
mulib_mune_state_t *mulib_mune_state_create(const mulib_mune_runtime_options_t*);
void mulib_mune_state_destroy(mulib_mune_state_t*);
mulib_mune_status_t mulib_mune_bind(mulib_mune_state_t*,const char*,mulib_mune_native_fn,void*);
mulib_mune_status_t mulib_mune_run_source(mulib_mune_state_t*,const char*,size_t,mulib_mune_result_t**,mulib_mune_error_t*);
size_t mulib_mune_result_count(const mulib_mune_result_t*);
mulib_mune_value_t mulib_mune_result_get(const mulib_mune_result_t*,size_t);
int mulib_mune_result_push(mulib_mune_result_t*,mulib_mune_value_t);
void mulib_mune_result_destroy(mulib_mune_result_t*);
const char *mulib_mune_value_string(mulib_mune_value_t,size_t*);
mulib_mune_status_t mulib_mune_make_string(mulib_mune_state_t*,const char*,size_t,mulib_mune_value_t*);
mulib_mune_status_t mulib_mune_get_global(mulib_mune_state_t*,const char*,mulib_mune_value_t*);
mulib_mune_status_t mulib_mune_call(mulib_mune_state_t*,mulib_mune_value_t,const mulib_mune_value_t*,size_t,mulib_mune_result_t**,mulib_mune_error_t*);
mulib_mune_value_t mulib_mune_nil(void);
mulib_mune_value_t mulib_mune_boolean(int);
mulib_mune_value_t mulib_mune_integer(int64_t);
#endif
