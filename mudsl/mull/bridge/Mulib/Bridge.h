#ifndef MULL_MULIB_BRIDGE_H
#define MULL_MULIB_BRIDGE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Stable, host-language-neutral ABI. IDs are local to this compiled bridge. */
enum mull_mb_kind { MULL_MB_VOID, MULL_MB_SIGNED, MULL_MB_UNSIGNED,
  MULL_MB_FLOAT, MULL_MB_POINTER, MULL_MB_ARRAY, MULL_MB_RECORD,
  MULL_MB_FUNCTION, MULL_MB_OPAQUE };
#define MULL_MB_NONE ((size_t)-1)
size_t mull_mb_library_count(void);
const char *mull_mb_library_name(size_t);
const char *mull_mb_library_header(size_t);
size_t mull_mb_library_find(const char *);
size_t mull_mb_symbol_count(size_t library);
const char *mull_mb_symbol_name(size_t library, size_t index);
/* Symbol kind: 0 function, 1 type, 2 constant. */
int mull_mb_symbol_find(size_t library, const char *name, size_t *id);
size_t mull_mb_type_find(size_t library, const char *name);
const char *mull_mb_type_name(size_t);
int mull_mb_type_kind(size_t);
size_t mull_mb_type_size(size_t);
size_t mull_mb_type_align(size_t);
size_t mull_mb_type_element(size_t);
size_t mull_mb_type_count(size_t);
int mull_mb_type_pointee_const(size_t);
size_t mull_mb_field_count(size_t);
const char *mull_mb_field_name(size_t type, size_t field);
size_t mull_mb_field_type(size_t type, size_t field);
int mull_mb_field_readonly(size_t type, size_t field);
void *mull_mb_field_address(size_t type, size_t field, void *object);
int mull_mb_field_get(size_t type, size_t field, const void *object, void *out);
int mull_mb_field_set(size_t type, size_t field, void *object, const void *value);
/* Exact integers use decimal text; number conversion rejects lossy integers. */
int mull_mb_number_read(size_t type, const void *value, double *out);
int mull_mb_number_write(size_t type, void *value, double number);
int mull_mb_integer_read(size_t type, const void *value, char *out, size_t capacity);
int mull_mb_integer_write(size_t type, void *value, const char *text);
size_t mull_mb_function_type(size_t function);
size_t mull_mb_result_type(size_t signature);
size_t mull_mb_argument_count(size_t signature);
size_t mull_mb_argument_type(size_t signature, size_t index);
void mull_mb_invoke(size_t function, void *result, void **arguments);
void mull_mb_invoke_pointer(size_t signature, const void *function, void *result, void **arguments);
size_t mull_mb_constant_type(size_t constant);
const void *mull_mb_constant_data(size_t constant);
void *mull_mb_allocate(size_t type, size_t count);
void mull_mb_deallocate(void *);
void mull_mb_pointer_write(void *storage, void *pointer);
void *mull_mb_pointer_read(const void *storage);
typedef void (*mull_mb_callback_fn)(void *user, size_t signature, void *result, void **arguments);
/* Callback slots are typed C trampolines, so unions and struct returns use
 * the compiler's ABI. Release only after native users have stopped calling. */
void *mull_mb_callback_create(size_t signature, mull_mb_callback_fn, void *user, void *function_out);
void mull_mb_callback_release(void *token);
/* Runtime-scoped backing storage. C owns the allocation list so D finalizers
 * can release it without traversing garbage-collected objects. */
typedef struct mull_mb_arena mull_mb_arena;
mull_mb_arena *mull_mb_arena_create(size_t max_bytes);
void mull_mb_arena_destroy(mull_mb_arena *);
void *mull_mb_arena_allocate(mull_mb_arena *, size_t type, size_t count);
int mull_mb_arena_callback(mull_mb_arena *, size_t signature, mull_mb_callback_fn, void *user, void *out);
#ifdef __cplusplus
}
#endif
#endif
