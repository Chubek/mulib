/*
 * On1xRT application embedding ABI 1.
 *
 * This is separate from the compiler-extension SDK and from the private
 * generated-program runtime ABI.  Native AOT module callbacks execute in the
 * host process and are not sandboxed.  Descriptors are copied at registration;
 * callback code and provider userdata remain provider-owned until unload.
 *
 * Every extensible descriptor begins with abi_version and struct_size.  A
 * callee reads only fields wholly covered by struct_size, treats absent
 * trailing fields as zero, and ignores unknown trailing fields for ABI 1.
 */
#ifndef ON1X_RT_H
#define ON1X_RT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_RT_ABI_VERSION 1u
#define ON1X_RT_MODULE_DESCRIPTOR_VERSION 1u
#define ON1X_RT_MAX_NAME_BYTES 128u
#define ON1X_RT_MAX_PARAMETERS 32u
#define ON1X_RT_MAX_EXPORTS 256u
#define ON1X_RT_MAX_DEPENDENCIES 64u

#if defined(_WIN32)
#define ON1X_RT_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define ON1X_RT_EXPORT __attribute__((visibility("default")))
#else
#define ON1X_RT_EXPORT
#endif

#ifdef __cplusplus
typedef enum On1xRT_Status : uint32_t {
#else
typedef enum On1xRT_Status {
#endif
  ON1X_RT_OK = 0,
  ON1X_RT_ERROR = 1,
  ON1X_RT_UNSUPPORTED = 2,
  ON1X_RT_LIMIT = 3,
  ON1X_RT_INVALID_ARGUMENT = 4,
  ON1X_RT_ABI_MISMATCH = 5,
  ON1X_RT_NOT_FOUND = 6,
  ON1X_RT_BUSY = 7,
  ON1X_RT_STALE_HANDLE = 8,
  ON1X_RT_WRONG_KIND = 9,
  ON1X_RT_WRONG_OWNER = 10,
  ON1X_RT_INVALID_STATE = 11,
  ON1X_RT_SIGNATURE_MISMATCH = 12,
  ON1X_RT_CALLBACK_FAILED = 13,
  ON1X_RT_WRONG_THREAD = 14,
  ON1X_RT_REENTRANT = 15,
  ON1X_RT_OUT_OF_MEMORY = 16,
  ON1X_RT_DENIED = 17
} On1xRT_Status;

typedef struct On1xRT_Engine On1xRT_Engine;
typedef struct On1xRT_Error On1xRT_Error;
typedef struct On1xRT_CallContext On1xRT_CallContext;
typedef struct On1xRT_ModuleContext On1xRT_ModuleContext;
typedef struct On1xRT_ModuleDescriptor On1xRT_ModuleDescriptor;

#define ON1X_RT_MODULE_ENTRY_SYMBOL on1x_rt_module_descriptor_v1
#define ON1X_RT_MODULE_ENTRY_SYMBOL_NAME "on1x_rt_module_descriptor_v1"

typedef struct On1xRT_StringView {
  const char *data;
  size_t size;
} On1xRT_StringView;

typedef struct On1xRT_BytesView {
  const uint8_t *data;
  size_t size;
} On1xRT_BytesView;

/*
 * Every serialized handle has generation32-slot32 in id.  owner is a
 * pointer-free engine-domain identity used in addition to generation, kind and
 * lifecycle validation.
 */
#define ON1X_RT_HANDLE_TYPE(Name)                                              \
  typedef struct Name {                                                        \
    uint64_t id;                                                               \
    uint64_t owner;                                                            \
  } Name
ON1X_RT_HANDLE_TYPE(On1xRT_Module);
ON1X_RT_HANDLE_TYPE(On1xRT_Instance);
ON1X_RT_HANDLE_TYPE(On1xRT_Function);
ON1X_RT_HANDLE_TYPE(On1xRT_String);
ON1X_RT_HANDLE_TYPE(On1xRT_Buffer);
ON1X_RT_HANDLE_TYPE(On1xRT_HostFunction);
#undef ON1X_RT_HANDLE_TYPE

typedef void *(*On1xRT_AllocateFn)(void *context, size_t size,
                                   size_t alignment);
typedef void *(*On1xRT_ReallocateFn)(void *context, void *pointer,
                                     size_t old_size, size_t new_size,
                                     size_t alignment);
typedef void (*On1xRT_ReleaseFn)(void *context, void *pointer, size_t size,
                                 size_t alignment);

typedef struct On1xRT_Allocator {
  uint32_t abi_version;
  uint32_t struct_size;
  void *context;
  On1xRT_AllocateFn allocate;
  On1xRT_ReallocateFn reallocate;
  On1xRT_ReleaseFn release;
} On1xRT_Allocator;

typedef On1xRT_Status (*On1xRT_AuthorizeAOTFn)(
    void *context, const On1xRT_ModuleDescriptor *descriptor);

enum { ON1X_RT_ENGINE_THREAD_AFFINE = 1u << 0 };

typedef struct On1xRT_EngineConfig {
  uint32_t abi_version;
  uint32_t struct_size;
  const On1xRT_Allocator *allocator;
  uint32_t max_handles;
  uint32_t max_modules;
  uint32_t max_host_functions;
  size_t max_value_bytes;
  uint32_t flags;
  uint32_t reserved;
  void *authorization_context;
  On1xRT_AuthorizeAOTFn authorize_aot;
} On1xRT_EngineConfig;

/*
 * Engines and every object they own are thread-confined to the creating
 * thread. Destroy rejects while any owned handle or registered module/host
 * function is live. On success it clears the caller's pointer.
 */
On1xRT_Status on1x_rt_engine_create(const On1xRT_EngineConfig *config,
                                    On1xRT_Engine **engine,
                                    On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_destroy(On1xRT_Engine **engine,
                                     On1xRT_Error **error);

uint32_t on1x_rt_error_code(const On1xRT_Error *error);
On1xRT_StringView on1x_rt_error_message(const On1xRT_Error *error);
On1xRT_Status on1x_rt_error_release(On1xRT_Error **error);
const char *on1x_rt_status_name(On1xRT_Status status);

#ifdef __cplusplus
typedef enum On1xRT_ValueKind : uint32_t {
#else
typedef enum On1xRT_ValueKind {
#endif
  ON1X_RT_VALUE_VOID = 0,
  ON1X_RT_VALUE_I64 = 1,
  ON1X_RT_VALUE_BOOL = 2,
  ON1X_RT_VALUE_F64 = 3,
  ON1X_RT_VALUE_STRING = 4,
  ON1X_RT_VALUE_BUFFER = 5
} On1xRT_ValueKind;

typedef struct On1xRT_Value {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xRT_ValueKind kind;
  uint32_t flags;
  union {
    int64_t i64;
    uint32_t boolean;
    double f64;
    On1xRT_String string;
    On1xRT_Buffer buffer;
  } as;
} On1xRT_Value;

typedef struct On1xRT_Signature {
  uint32_t abi_version;
  uint32_t struct_size;
  const On1xRT_ValueKind *parameters;
  size_t parameter_count;
  On1xRT_ValueKind result;
  uint32_t flags;
} On1xRT_Signature;

On1xRT_Value on1x_rt_value_void(void);
On1xRT_Value on1x_rt_value_i64(int64_t value);
On1xRT_Value on1x_rt_value_bool(int value);
On1xRT_Value on1x_rt_value_f64(double value);
/*
 * String/buffer constructors copy a handle token but do not retain it.
 * Function arguments borrow that token for the synchronous call.  An owned
 * result token is released exactly once with on1x_rt_value_release (or the
 * corresponding handle release); copied aliases must not be released twice.
 */
On1xRT_Value on1x_rt_value_string(On1xRT_String value);
On1xRT_Value on1x_rt_value_buffer(On1xRT_Buffer value);
On1xRT_Status on1x_rt_value_release(On1xRT_Engine *engine, On1xRT_Value *value,
                                    On1xRT_Error **error);
int on1x_rt_signature_equal(const On1xRT_Signature *left,
                            const On1xRT_Signature *right);

typedef On1xRT_Status (*On1xRT_NativeFunction)(On1xRT_CallContext *call,
                                               void *function_context,
                                               const On1xRT_Value *arguments,
                                               size_t argument_count,
                                               On1xRT_Value *result,
                                               On1xRT_Error **error);

typedef struct On1xRT_ExportDescriptor {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xRT_StringView name;
  On1xRT_Signature signature;
  On1xRT_NativeFunction invoke;
  void *function_context;
  uint32_t flags;
  uint32_t reserved;
} On1xRT_ExportDescriptor;

typedef On1xRT_Status (*On1xRT_ModuleInitializeFn)(
    On1xRT_ModuleContext *context, void *provider_context, void **module_state,
    On1xRT_Error **error);
typedef On1xRT_Status (*On1xRT_ModuleShutdownFn)(On1xRT_ModuleContext *context,
                                                 void *provider_context,
                                                 void *module_state,
                                                 On1xRT_Error **error);
typedef On1xRT_Status (*On1xRT_InstanceCreateFn)(On1xRT_ModuleContext *context,
                                                 void *provider_context,
                                                 void *module_state,
                                                 void **instance_state,
                                                 On1xRT_Error **error);
typedef On1xRT_Status (*On1xRT_InstanceDestroyFn)(On1xRT_ModuleContext *context,
                                                  void *provider_context,
                                                  void *module_state,
                                                  void *instance_state,
                                                  On1xRT_Error **error);

/*
 * A failing initialize/create callback must leave its output state null.
 * Successful initialization participates in transaction rollback, so shutdown
 * must be able to compensate it.  A public unload/destroy callback that fails
 * leaves its handle live and may be retried; such failure must be atomic.
 */

enum {
  ON1X_RT_MODULE_STATIC = 1u << 0,
  ON1X_RT_MODULE_AUTHORIZED_AOT = 1u << 1
};

struct On1xRT_ModuleDescriptor {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t descriptor_version;
  uint32_t flags;
  On1xRT_StringView name;
  On1xRT_StringView revision;
  On1xRT_StringView identity_sha256;
  const On1xRT_StringView *dependencies;
  size_t dependency_count;
  const On1xRT_ExportDescriptor *exports;
  size_t export_count;
  void *provider_context;
  On1xRT_ModuleInitializeFn initialize;
  On1xRT_ModuleShutdownFn shutdown;
  On1xRT_InstanceCreateFn instance_create;
  On1xRT_InstanceDestroyFn instance_destroy;
};

typedef const On1xRT_ModuleDescriptor *(*On1xRT_ModuleEntryFn)(void);

/*
 * Validation/discovery never invokes callbacks.  Registration copies all
 * strings, dependency arrays, exports and signatures before initialization.
 */
On1xRT_Status
on1x_rt_module_descriptor_check(const On1xRT_ModuleDescriptor *descriptor,
                                On1xRT_Error **error);

On1xRT_Status on1x_rt_engine_register_static_modules(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *const *descriptors,
    size_t count, On1xRT_Module *modules, size_t module_capacity,
    On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_register_aot_modules(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *const *descriptors,
    size_t count, On1xRT_Module *modules, size_t module_capacity,
    On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_register_static_module(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *descriptor,
    On1xRT_Module *module, On1xRT_Error **error);
On1xRT_Status
on1x_rt_engine_register_aot_module(On1xRT_Engine *engine,
                                   const On1xRT_ModuleDescriptor *descriptor,
                                   On1xRT_Module *module, On1xRT_Error **error);

On1xRT_Status on1x_rt_module_unload(On1xRT_Engine *engine,
                                    On1xRT_Module *module,
                                    On1xRT_Error **error);
On1xRT_Status on1x_rt_instance_create(On1xRT_Engine *engine,
                                      On1xRT_Module module,
                                      On1xRT_Instance *instance,
                                      On1xRT_Error **error);
On1xRT_Status on1x_rt_instance_release(On1xRT_Engine *engine,
                                       On1xRT_Instance *instance,
                                       On1xRT_Error **error);
On1xRT_Status
on1x_rt_instance_function(On1xRT_Engine *engine, On1xRT_Instance instance,
                          On1xRT_StringView name,
                          const On1xRT_Signature *expected_signature,
                          On1xRT_Function *function, On1xRT_Error **error);
On1xRT_Status on1x_rt_function_release(On1xRT_Engine *engine,
                                       On1xRT_Function *function,
                                       On1xRT_Error **error);
/*
 * arguments and their referenced handles are borrowed for the call.  result is
 * overwritten and therefore must not contain an unreleased owned value.  A
 * successful string/buffer result owns one handle for the caller.
 */
On1xRT_Status on1x_rt_function_call(On1xRT_Engine *engine,
                                    On1xRT_Function function,
                                    const On1xRT_Value *arguments,
                                    size_t argument_count, On1xRT_Value *result,
                                    On1xRT_Error **error);

On1xRT_Status on1x_rt_string_create(On1xRT_Engine *engine, const void *bytes,
                                    size_t size, On1xRT_String *string,
                                    On1xRT_Error **error);
On1xRT_Status on1x_rt_string_view(On1xRT_Engine *engine, On1xRT_String string,
                                  On1xRT_BytesView *view, On1xRT_Error **error);
On1xRT_Status on1x_rt_string_release(On1xRT_Engine *engine,
                                     On1xRT_String *string,
                                     On1xRT_Error **error);

On1xRT_Status on1x_rt_buffer_create(On1xRT_Engine *engine, const void *bytes,
                                    size_t size, On1xRT_Buffer *buffer,
                                    On1xRT_Error **error);
On1xRT_Status on1x_rt_buffer_view(On1xRT_Engine *engine, On1xRT_Buffer buffer,
                                  On1xRT_BytesView *view, On1xRT_Error **error);
On1xRT_Status on1x_rt_buffer_write(On1xRT_Engine *engine, On1xRT_Buffer buffer,
                                   size_t offset, const void *bytes,
                                   size_t size, On1xRT_Error **error);
On1xRT_Status on1x_rt_buffer_release(On1xRT_Engine *engine,
                                     On1xRT_Buffer *buffer,
                                     On1xRT_Error **error);

typedef struct On1xRT_HostFunctionDescriptor {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xRT_StringView name;
  On1xRT_Signature signature;
  On1xRT_NativeFunction invoke;
  void *function_context;
  uint32_t flags;
  uint32_t reserved;
} On1xRT_HostFunctionDescriptor;

On1xRT_Status on1x_rt_engine_register_host_function(
    On1xRT_Engine *engine, const On1xRT_HostFunctionDescriptor *descriptor,
    On1xRT_HostFunction *function, On1xRT_Error **error);
On1xRT_Status on1x_rt_host_function_unregister(On1xRT_Engine *engine,
                                               On1xRT_HostFunction *function,
                                               On1xRT_Error **error);

/*
 * Call/module contexts are callback-scoped borrowed capabilities.  They cannot
 * be retained.  Result constructors allocate in the engine's allocator domain.
 */
On1xRT_Status on1x_rt_call_string_view(On1xRT_CallContext *call,
                                       On1xRT_String string,
                                       On1xRT_BytesView *view,
                                       On1xRT_Error **error);
On1xRT_Status on1x_rt_call_buffer_view(On1xRT_CallContext *call,
                                       On1xRT_Buffer buffer,
                                       On1xRT_BytesView *view,
                                       On1xRT_Error **error);
On1xRT_Status on1x_rt_call_string_create(On1xRT_CallContext *call,
                                         const void *bytes, size_t size,
                                         On1xRT_String *string,
                                         On1xRT_Error **error);
On1xRT_Status on1x_rt_call_buffer_create(On1xRT_CallContext *call,
                                         const void *bytes, size_t size,
                                         On1xRT_Buffer *buffer,
                                         On1xRT_Error **error);
On1xRT_Status on1x_rt_call_host(On1xRT_CallContext *call,
                                On1xRT_StringView name,
                                const On1xRT_Signature *signature,
                                const On1xRT_Value *arguments,
                                size_t argument_count, On1xRT_Value *result,
                                On1xRT_Error **error);
On1xRT_Status on1x_rt_call_value_release(On1xRT_CallContext *call,
                                         On1xRT_Value *value,
                                         On1xRT_Error **error);
On1xRT_Status on1x_rt_call_error(On1xRT_CallContext *call, uint32_t code,
                                 On1xRT_StringView message,
                                 On1xRT_Error **error);
void *on1x_rt_call_module_state(On1xRT_CallContext *call);
void *on1x_rt_call_instance_state(On1xRT_CallContext *call);

void *on1x_rt_module_allocate(On1xRT_ModuleContext *context, size_t size,
                              size_t alignment);
void on1x_rt_module_release_memory(On1xRT_ModuleContext *context, void *pointer,
                                   size_t size, size_t alignment);
On1xRT_Status on1x_rt_module_error(On1xRT_ModuleContext *context, uint32_t code,
                                   On1xRT_StringView message,
                                   On1xRT_Error **error);

#ifdef __cplusplus
typedef enum On1xRT_Feature : uint32_t {
#else
typedef enum On1xRT_Feature {
#endif
  ON1X_RT_FEATURE_DYNAMIC_MODULE = 1,
  ON1X_RT_FEATURE_WASM = 2,
  ON1X_RT_FEATURE_CONCURRENCY = 3,
  ON1X_RT_FEATURE_TASKS = 4,
  ON1X_RT_FEATURE_FIBERS = 5,
  ON1X_RT_FEATURE_CHANNELS = 6,
  ON1X_RT_FEATURE_SCHEDULER = 7,
  ON1X_RT_FEATURE_SANDBOX = 8,
  ON1X_RT_FEATURE_DEBUGGER = 9,
  ON1X_RT_FEATURE_PROFILER = 10
} On1xRT_Feature;

On1xRT_Status on1x_rt_feature_status(On1xRT_Feature feature);
On1xRT_Status on1x_rt_engine_load_dynamic(On1xRT_Engine *engine,
                                          On1xRT_StringView path,
                                          On1xRT_Module *module,
                                          On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_load_wasm(On1xRT_Engine *engine,
                                       On1xRT_BytesView module_bytes,
                                       On1xRT_Module *module,
                                       On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_enable_concurrency(On1xRT_Engine *engine,
                                                On1xRT_Error **error);
On1xRT_Status on1x_rt_engine_attach_debugger(On1xRT_Engine *engine,
                                             On1xRT_Error **error);

#ifndef __cplusplus
_Static_assert(sizeof(On1xRT_Status) == sizeof(uint32_t),
               "On1xRT requires 32-bit status enums");
_Static_assert(sizeof(On1xRT_ValueKind) == sizeof(uint32_t),
               "On1xRT requires 32-bit value-kind enums");
_Static_assert(sizeof(On1xRT_Feature) == sizeof(uint32_t),
               "On1xRT requires 32-bit feature enums");
#endif

#ifdef __cplusplus
}
#endif

#endif
