#include "On1xRT.h"
#include "On1xRT-Dynamic.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define ON1X_RT_ENGINE_MAGIC UINT64_C(0x4f4e31585254454e)
#define ON1X_RT_ERROR_MAGIC UINT64_C(0x4f4e315852544552)
#define ON1X_RT_CALL_MAGIC UINT64_C(0x4f4e315852544341)
#define ON1X_RT_MODULE_CONTEXT_MAGIC UINT64_C(0x4f4e315852544d43)
#define ON1X_RT_MAX_ERROR_BYTES 4096u
#define ON1X_RT_MAX_HANDLES_LIMIT 1048576u
#define ON1X_RT_MAX_MODULES_LIMIT 65536u
#define ON1X_RT_MAX_HOST_FUNCTIONS_LIMIT 65536u
#define ON1X_RT_MAX_VALUE_BYTES_LIMIT ((size_t)1u << 30)
#define ON1X_RT_MAX_ALIGNMENT 4096u

#define ON1X_RT_REQUIRED(T, field)                                             \
  ((uint32_t)(offsetof(T, field) + sizeof(((T *)0)->field)))
#define ON1X_RT_HAS(value, T, field)                                           \
  ((value) != NULL && (value)->struct_size >= ON1X_RT_REQUIRED(T, field))

typedef enum On1xRT_SlotKind {
  ON1X_RT_SLOT_NONE = 0,
  ON1X_RT_SLOT_MODULE = 1,
  ON1X_RT_SLOT_INSTANCE = 2,
  ON1X_RT_SLOT_FUNCTION = 3,
  ON1X_RT_SLOT_STRING = 4,
  ON1X_RT_SLOT_BUFFER = 5,
  ON1X_RT_SLOT_HOST_FUNCTION = 6
} On1xRT_SlotKind;

typedef struct On1xRT_Slot {
  uint32_t generation;
  uint16_t kind;
  uint16_t retired;
  uint32_t references;
  void *object;
} On1xRT_Slot;

typedef struct On1xRT_ByteObject {
  uint8_t *data;
  size_t size;
  uint64_t pending_call;
} On1xRT_ByteObject;

typedef struct On1xRT_ModuleObject On1xRT_ModuleObject;

typedef struct On1xRT_InstanceObject {
  uint32_t module_slot;
  uint32_t module_generation;
  void *state;
  size_t function_count;
} On1xRT_InstanceObject;

typedef struct On1xRT_FunctionObject {
  uint32_t instance_slot;
  uint32_t instance_generation;
  size_t export_index;
} On1xRT_FunctionObject;

typedef struct On1xRT_HostObject {
  char *name;
  size_t name_size;
  On1xRT_Signature signature;
  On1xRT_NativeFunction invoke;
  void *function_context;
  unsigned active;
} On1xRT_HostObject;

struct On1xRT_ModuleObject {
  On1xRT_ModuleDescriptor descriptor;
  char *name;
  char *revision;
  char *identity;
  On1xRT_StringView *dependencies;
  On1xRT_ExportDescriptor *exports;
  void *state;
  size_t instance_count;
  int initialized;
#ifdef ON1X_RT_HAS_DYNAMIC
  void *native_library;
  int native_fd;
  size_t native_size;
#endif
};

struct On1xRT_Error {
  uint64_t magic;
  On1xRT_Allocator allocator;
  uint32_t code;
  uint32_t reserved;
  size_t size;
  char message[];
};

struct On1xRT_Engine {
  uint64_t magic;
  uint64_t owner;
  On1xRT_Allocator allocator;
  On1xRT_Slot *slots;
  uint32_t max_handles;
  uint32_t max_modules;
  uint32_t max_host_functions;
  uint32_t module_count;
  uint32_t host_function_count;
  uint32_t live_handles;
  size_t max_value_bytes;
  size_t value_bytes;
  thrd_t thread;
  unsigned callback_depth;
  uint64_t next_call;
  void *authorization_context;
  On1xRT_AuthorizeAOTFn authorize_aot;
#ifdef ON1X_RT_HAS_DYNAMIC
  size_t native_bytes;
#endif
};

struct On1xRT_CallContext {
  uint64_t magic;
  On1xRT_Engine *engine;
  On1xRT_ModuleObject *module;
  On1xRT_InstanceObject *instance;
  uint64_t call_id;
  unsigned active;
  unsigned expose_module_state;
};

struct On1xRT_ModuleContext {
  uint64_t magic;
  On1xRT_Engine *engine;
  On1xRT_ModuleObject *module;
  unsigned active;
};

typedef struct On1xRT_DefaultHeader {
  void *base;
} On1xRT_DefaultHeader;

static _Atomic uint64_t on1x_rt_next_owner_ = UINT64_C(1);

static uint64_t on1x_rt_owner_acquire_(void) {
  uint64_t current =
      atomic_load_explicit(&on1x_rt_next_owner_, memory_order_relaxed);
  while (current) {
    const uint64_t next =
        current == UINT64_MAX ? UINT64_C(0) : current + UINT64_C(1);
    if (atomic_compare_exchange_weak_explicit(&on1x_rt_next_owner_, &current,
                                              next, memory_order_relaxed,
                                              memory_order_relaxed))
      return current;
  }
  return UINT64_C(0);
}

static int on1x_rt_power_of_two_(size_t value) {
  return value && !(value & (value - 1u));
}

static int on1x_rt_alignment_valid_(size_t alignment) {
  return on1x_rt_power_of_two_(alignment) && alignment <= ON1X_RT_MAX_ALIGNMENT;
}

static void *on1x_rt_default_allocate_(void *context, size_t size,
                                       size_t alignment) {
  uintptr_t address;
  void *base;
  size_t amount;
  (void)context;
  if (!size)
    size = 1u;
  if (!on1x_rt_alignment_valid_(alignment) ||
      size > SIZE_MAX - alignment - sizeof(On1xRT_DefaultHeader)) {
    return NULL;
  }
  amount = size + alignment - 1u + sizeof(On1xRT_DefaultHeader);
  base = malloc(amount);
  if (!base)
    return NULL;
  address = (uintptr_t)base + sizeof(On1xRT_DefaultHeader);
  address = (address + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
  ((On1xRT_DefaultHeader *)address)[-1].base = base;
  return (void *)address;
}

static void on1x_rt_default_release_(void *context, void *pointer, size_t size,
                                     size_t alignment) {
  (void)context;
  (void)size;
  (void)alignment;
  if (pointer)
    free(((On1xRT_DefaultHeader *)pointer)[-1].base);
}

static void *on1x_rt_default_reallocate_(void *context, void *pointer,
                                         size_t old_size, size_t new_size,
                                         size_t alignment) {
  void *replacement;
  if (!pointer)
    return on1x_rt_default_allocate_(context, new_size, alignment);
  if (!new_size) {
    on1x_rt_default_release_(context, pointer, old_size, alignment);
    return NULL;
  }
  replacement = on1x_rt_default_allocate_(context, new_size, alignment);
  if (!replacement)
    return NULL;
  memcpy(replacement, pointer, old_size < new_size ? old_size : new_size);
  on1x_rt_default_release_(context, pointer, old_size, alignment);
  return replacement;
}

static On1xRT_Allocator on1x_rt_default_allocator_(void) {
  On1xRT_Allocator allocator;
  memset(&allocator, 0, sizeof(allocator));
  allocator.abi_version = ON1X_RT_ABI_VERSION;
  allocator.struct_size = sizeof(allocator);
  allocator.allocate = on1x_rt_default_allocate_;
  allocator.reallocate = on1x_rt_default_reallocate_;
  allocator.release = on1x_rt_default_release_;
  return allocator;
}

static int on1x_rt_prefix_(const void *value, uint32_t size,
                           uint32_t required) {
  const uint32_t *prefix = (const uint32_t *)value;
  return value && size >= 2u * sizeof(uint32_t) &&
         prefix[0] == ON1X_RT_ABI_VERSION && prefix[1] >= required;
}

static int on1x_rt_allocator_valid_(const On1xRT_Allocator *allocator) {
  return allocator &&
         on1x_rt_prefix_(allocator, allocator->struct_size,
                         ON1X_RT_REQUIRED(On1xRT_Allocator, release)) &&
         allocator->allocate && allocator->reallocate && allocator->release;
}

static void *on1x_rt_allocate_(const On1xRT_Allocator *allocator, size_t size,
                               size_t alignment) {
  void *pointer;
  if (!allocator || !size || !on1x_rt_alignment_valid_(alignment))
    return NULL;
  pointer = allocator->allocate(allocator->context, size, alignment);
  if (pointer && ((uintptr_t)pointer & (alignment - 1u))) {
    allocator->release(allocator->context, pointer, size, alignment);
    return NULL;
  }
  return pointer;
}

static void on1x_rt_release_(const On1xRT_Allocator *allocator, void *pointer,
                             size_t size, size_t alignment) {
  if (pointer)
    allocator->release(allocator->context, pointer, size, alignment);
}

static int on1x_rt_utf8_(const uint8_t *data, size_t size) {
  size_t index = 0;
  while (index < size) {
    uint32_t codepoint;
    unsigned needed;
    const uint8_t first = data[index++];
    if (first < 0x80u)
      continue;
    if (first >= 0xc2u && first <= 0xdfu) {
      codepoint = first & 0x1fu;
      needed = 1u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      codepoint = first & 0x0fu;
      needed = 2u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      codepoint = first & 0x07u;
      needed = 3u;
    } else {
      return 0;
    }
    if (needed > size - index)
      return 0;
    for (unsigned part = 0; part < needed; ++part) {
      const uint8_t byte = data[index++];
      if ((byte & 0xc0u) != 0x80u)
        return 0;
      codepoint = (codepoint << 6) | (uint32_t)(byte & 0x3fu);
    }
    if ((needed == 2u && codepoint < 0x800u) ||
        (needed == 3u && codepoint < 0x10000u) || codepoint > 0x10ffffu ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
      return 0;
    }
  }
  return 1;
}

static int on1x_rt_view_valid_(On1xRT_StringView view, size_t maximum,
                               int allow_empty, int allow_nul) {
  if (view.size > maximum || (!view.data && view.size) ||
      (!allow_empty && !view.size))
    return 0;
  if (!allow_nul && view.size && memchr(view.data, 0, view.size))
    return 0;
  return !view.size || on1x_rt_utf8_((const uint8_t *)view.data, view.size);
}

static int on1x_rt_name_valid_(On1xRT_StringView name) {
  if (!on1x_rt_view_valid_(name, ON1X_RT_MAX_NAME_BYTES, 0, 0))
    return 0;
  for (size_t index = 0; index < name.size; ++index) {
    const unsigned char byte = (unsigned char)name.data[index];
    if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
          (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' ||
          byte == '.')) {
      return 0;
    }
  }
  return 1;
}

static int on1x_rt_view_equal_(On1xRT_StringView left,
                               On1xRT_StringView right) {
  return left.size == right.size &&
         (!left.size || !memcmp(left.data, right.data, left.size));
}

static int on1x_rt_sha256_valid_(On1xRT_StringView digest) {
  if (!digest.data || digest.size != 64u)
    return 0;
  for (size_t index = 0; index < digest.size; ++index) {
    const unsigned char byte = (unsigned char)digest.data[index];
    if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f')))
      return 0;
  }
  return 1;
}

static int on1x_rt_error_ready_(On1xRT_Error **error) {
  return !error || !*error;
}

static On1xRT_Error *on1x_rt_make_error_(const On1xRT_Allocator *allocator,
                                         uint32_t code, const char *message,
                                         size_t size) {
  On1xRT_Error *error;
  size_t amount;
  if (!allocator || !message || size > ON1X_RT_MAX_ERROR_BYTES ||
      size > SIZE_MAX - sizeof(*error) - 1u)
    return NULL;
  amount = sizeof(*error) + size + 1u;
  error = (On1xRT_Error *)on1x_rt_allocate_(allocator, amount,
                                            alignof(On1xRT_Error));
  if (!error)
    return NULL;
  memset(error, 0, sizeof(*error));
  error->magic = ON1X_RT_ERROR_MAGIC;
  error->allocator = *allocator;
  error->code = code;
  error->size = size;
  memcpy(error->message, message, size);
  error->message[size] = '\0';
  return error;
}

static On1xRT_Status on1x_rt_fail_(const On1xRT_Allocator *allocator,
                                   On1xRT_Error **error, On1xRT_Status status,
                                   const char *message) {
  if (error && !*error)
    *error = on1x_rt_make_error_(allocator, (uint32_t)status, message,
                                 strlen(message));
  return status;
}

static On1xRT_Status on1x_rt_default_fail_(On1xRT_Error **error,
                                           On1xRT_Status status,
                                           const char *message) {
  const On1xRT_Allocator allocator = on1x_rt_default_allocator_();
  return on1x_rt_fail_(&allocator, error, status, message);
}

static On1xRT_Status on1x_rt_engine_fail_(On1xRT_Engine *engine,
                                          On1xRT_Error **error,
                                          On1xRT_Status status,
                                          const char *message) {
  /* A host allocator is not assumed safe on a non-owning thread. */
  if (status == ON1X_RT_WRONG_THREAD)
    return status;
  return on1x_rt_fail_(&engine->allocator, error, status, message);
}

static On1xRT_Status on1x_rt_status_normalize_(On1xRT_Status status) {
  const uint32_t numeric = (uint32_t)status;
  return numeric <= (uint32_t)ON1X_RT_DENIED ? status : ON1X_RT_ERROR;
}

uint32_t on1x_rt_error_code(const On1xRT_Error *error) {
  return error && error->magic == ON1X_RT_ERROR_MAGIC ? error->code : 0u;
}

On1xRT_StringView on1x_rt_error_message(const On1xRT_Error *error) {
  On1xRT_StringView view = {NULL, 0};
  if (error && error->magic == ON1X_RT_ERROR_MAGIC) {
    view.data = error->message;
    view.size = error->size;
  }
  return view;
}

On1xRT_Status on1x_rt_error_release(On1xRT_Error **error) {
  On1xRT_Error *object;
  On1xRT_Allocator allocator;
  size_t amount;
  if (!error || !(object = *error) || object->magic != ON1X_RT_ERROR_MAGIC)
    return ON1X_RT_INVALID_ARGUMENT;
  allocator = object->allocator;
  amount = sizeof(*object) + object->size + 1u;
  object->magic = 0;
  *error = NULL;
  on1x_rt_release_(&allocator, object, amount, alignof(On1xRT_Error));
  return ON1X_RT_OK;
}

const char *on1x_rt_status_name(On1xRT_Status status) {
  switch (status) {
  case ON1X_RT_OK:
    return "OK";
  case ON1X_RT_ERROR:
    return "ERROR";
  case ON1X_RT_UNSUPPORTED:
    return "UNSUPPORTED";
  case ON1X_RT_LIMIT:
    return "LIMIT";
  case ON1X_RT_INVALID_ARGUMENT:
    return "INVALID_ARGUMENT";
  case ON1X_RT_ABI_MISMATCH:
    return "ABI_MISMATCH";
  case ON1X_RT_NOT_FOUND:
    return "NOT_FOUND";
  case ON1X_RT_BUSY:
    return "BUSY";
  case ON1X_RT_STALE_HANDLE:
    return "STALE_HANDLE";
  case ON1X_RT_WRONG_KIND:
    return "WRONG_KIND";
  case ON1X_RT_WRONG_OWNER:
    return "WRONG_OWNER";
  case ON1X_RT_INVALID_STATE:
    return "INVALID_STATE";
  case ON1X_RT_SIGNATURE_MISMATCH:
    return "SIGNATURE_MISMATCH";
  case ON1X_RT_CALLBACK_FAILED:
    return "CALLBACK_FAILED";
  case ON1X_RT_WRONG_THREAD:
    return "WRONG_THREAD";
  case ON1X_RT_REENTRANT:
    return "REENTRANT";
  case ON1X_RT_OUT_OF_MEMORY:
    return "OUT_OF_MEMORY";
  case ON1X_RT_DENIED:
    return "DENIED";
  }
  return "UNKNOWN_STATUS";
}

static On1xRT_Status on1x_rt_engine_check_(On1xRT_Engine *engine,
                                           int allow_callback) {
  if (!engine || engine->magic != ON1X_RT_ENGINE_MAGIC)
    return ON1X_RT_INVALID_ARGUMENT;
  if (!thrd_equal(engine->thread, thrd_current()))
    return ON1X_RT_WRONG_THREAD;
  if (!allow_callback && engine->callback_depth)
    return ON1X_RT_REENTRANT;
  return ON1X_RT_OK;
}

static uint64_t on1x_rt_handle_id_(uint32_t slot, uint32_t generation) {
  return ((uint64_t)generation << 32) | (uint64_t)slot;
}

static uint32_t on1x_rt_handle_slot_(uint64_t id) {
  return (uint32_t)(id & UINT64_C(0xffffffff));
}

static uint32_t on1x_rt_handle_generation_(uint64_t id) {
  return (uint32_t)(id >> 32);
}

static On1xRT_Status on1x_rt_slot_allocate_(On1xRT_Engine *engine,
                                            On1xRT_SlotKind kind, void *object,
                                            uint32_t references, uint64_t *id) {
  for (uint32_t index = 0; index < engine->max_handles; ++index) {
    On1xRT_Slot *slot = engine->slots + index;
    if (slot->kind != ON1X_RT_SLOT_NONE || slot->retired)
      continue;
    if (!slot->generation)
      slot->generation = 1u;
    slot->kind = (uint16_t)kind;
    slot->object = object;
    slot->references = references;
    ++engine->live_handles;
    *id = on1x_rt_handle_id_(index, slot->generation);
    return ON1X_RT_OK;
  }
  return ON1X_RT_LIMIT;
}

static void on1x_rt_slot_clear_(On1xRT_Engine *engine, uint32_t index) {
  On1xRT_Slot *slot = engine->slots + index;
  slot->object = NULL;
  slot->kind = ON1X_RT_SLOT_NONE;
  slot->references = 0u;
  if (slot->generation == UINT32_MAX) {
    slot->retired = 1u;
  } else {
    ++slot->generation;
    if (!slot->generation)
      slot->retired = 1u;
  }
  --engine->live_handles;
}

static On1xRT_Status on1x_rt_slot_get_(On1xRT_Engine *engine, uint64_t id,
                                       uint64_t owner, On1xRT_SlotKind kind,
                                       On1xRT_Slot **slot, uint32_t *index) {
  uint32_t selected;
  if (slot)
    *slot = NULL;
  if (index)
    *index = 0u;
  if (!id || !owner)
    return ON1X_RT_INVALID_ARGUMENT;
  if (owner != engine->owner)
    return ON1X_RT_WRONG_OWNER;
  selected = on1x_rt_handle_slot_(id);
  if (selected >= engine->max_handles)
    return ON1X_RT_STALE_HANDLE;
  if (engine->slots[selected].kind == ON1X_RT_SLOT_NONE ||
      engine->slots[selected].generation != on1x_rt_handle_generation_(id))
    return ON1X_RT_STALE_HANDLE;
  if (engine->slots[selected].kind != (uint16_t)kind)
    return ON1X_RT_WRONG_KIND;
  if (slot)
    *slot = engine->slots + selected;
  if (index)
    *index = selected;
  return ON1X_RT_OK;
}

static uint32_t on1x_rt_free_slots_(const On1xRT_Engine *engine) {
  uint32_t count = 0;
  for (uint32_t index = 0; index < engine->max_handles; ++index)
    if (engine->slots[index].kind == ON1X_RT_SLOT_NONE &&
        !engine->slots[index].retired)
      ++count;
  return count;
}

On1xRT_Status on1x_rt_engine_create(const On1xRT_EngineConfig *config,
                                    On1xRT_Engine **engine,
                                    On1xRT_Error **error) {
  On1xRT_Allocator allocator = on1x_rt_default_allocator_();
  On1xRT_Engine *object;
  uint32_t max_handles = 1024u;
  uint32_t max_modules = 64u;
  uint32_t max_host_functions = 64u;
  size_t max_value_bytes = 16u * 1024u * 1024u;
  uint32_t flags = 0u;
  size_t slot_bytes;
  uint64_t owner;
  if (!engine || *engine || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  if (config) {
    if (!on1x_rt_prefix_(config, config->struct_size,
                         ON1X_RT_REQUIRED(On1xRT_EngineConfig, struct_size)))
      return on1x_rt_default_fail_(error,
                                   config->abi_version == ON1X_RT_ABI_VERSION
                                       ? ON1X_RT_INVALID_ARGUMENT
                                       : ON1X_RT_ABI_MISMATCH,
                                   "invalid engine configuration prefix");
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, allocator) &&
        config->allocator) {
      if (!on1x_rt_allocator_valid_(config->allocator))
        return on1x_rt_default_fail_(error, ON1X_RT_INVALID_ARGUMENT,
                                     "invalid host allocator descriptor");
      allocator = *config->allocator;
    }
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, max_handles) &&
        config->max_handles)
      max_handles = config->max_handles;
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, max_modules) &&
        config->max_modules)
      max_modules = config->max_modules;
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, max_host_functions) &&
        config->max_host_functions)
      max_host_functions = config->max_host_functions;
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, max_value_bytes) &&
        config->max_value_bytes)
      max_value_bytes = config->max_value_bytes;
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, flags))
      flags = config->flags;
    if (ON1X_RT_HAS(config, On1xRT_EngineConfig, reserved) && config->reserved)
      return on1x_rt_fail_(&allocator, error, ON1X_RT_INVALID_ARGUMENT,
                           "engine configuration reserved fields must be zero");
  }
  if ((flags & ~(uint32_t)ON1X_RT_ENGINE_THREAD_AFFINE) ||
      max_handles > ON1X_RT_MAX_HANDLES_LIMIT ||
      max_modules > ON1X_RT_MAX_MODULES_LIMIT ||
      max_host_functions > ON1X_RT_MAX_HOST_FUNCTIONS_LIMIT ||
      max_modules > max_handles || max_host_functions > max_handles ||
      !max_handles || !max_modules || !max_host_functions || !max_value_bytes ||
      max_value_bytes > ON1X_RT_MAX_VALUE_BYTES_LIMIT) {
    return on1x_rt_fail_(&allocator, error, ON1X_RT_LIMIT,
                         "engine limits are invalid or exceed hard bounds");
  }
  object = (On1xRT_Engine *)on1x_rt_allocate_(&allocator, sizeof(*object),
                                              alignof(On1xRT_Engine));
  if (!object)
    return on1x_rt_fail_(&allocator, error, ON1X_RT_OUT_OF_MEMORY,
                         "cannot allocate engine");
  memset(object, 0, sizeof(*object));
  object->allocator = allocator;
  slot_bytes = (size_t)max_handles * sizeof(On1xRT_Slot);
  object->slots = (On1xRT_Slot *)on1x_rt_allocate_(&allocator, slot_bytes,
                                                   alignof(On1xRT_Slot));
  if (!object->slots) {
    on1x_rt_release_(&allocator, object, sizeof(*object),
                     alignof(On1xRT_Engine));
    return on1x_rt_fail_(&allocator, error, ON1X_RT_OUT_OF_MEMORY,
                         "cannot allocate handle table");
  }
  memset(object->slots, 0, slot_bytes);
  owner = on1x_rt_owner_acquire_();
  if (!owner) {
    on1x_rt_release_(&allocator, object->slots, slot_bytes,
                     alignof(On1xRT_Slot));
    on1x_rt_release_(&allocator, object, sizeof(*object),
                     alignof(On1xRT_Engine));
    return on1x_rt_fail_(&allocator, error, ON1X_RT_LIMIT,
                         "engine owner identities are exhausted");
  }
  object->magic = ON1X_RT_ENGINE_MAGIC;
  object->owner = owner;
  object->max_handles = max_handles;
  object->max_modules = max_modules;
  object->max_host_functions = max_host_functions;
  object->max_value_bytes = max_value_bytes;
  object->thread = thrd_current();
  object->next_call = 1u;
  if (config && ON1X_RT_HAS(config, On1xRT_EngineConfig, authorize_aot)) {
    object->authorization_context = config->authorization_context;
    object->authorize_aot = config->authorize_aot;
  }
  *engine = object;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_engine_destroy(On1xRT_Engine **engine,
                                     On1xRT_Error **error) {
  On1xRT_Engine *object;
  On1xRT_Status status;
  On1xRT_Allocator allocator;
  size_t slot_bytes;
  if (!engine || !(object = *engine) || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(object, 0);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(object, error, status,
                                "engine cannot be destroyed in this context");
  if (object->live_handles)
    return on1x_rt_engine_fail_(
        object, error, ON1X_RT_BUSY,
        "engine still owns live handles, modules, or host functions");
  allocator = object->allocator;
  slot_bytes = (size_t)object->max_handles * sizeof(On1xRT_Slot);
  object->magic = 0u;
  *engine = NULL;
  on1x_rt_release_(&allocator, object->slots, slot_bytes, alignof(On1xRT_Slot));
  on1x_rt_release_(&allocator, object, sizeof(*object), alignof(On1xRT_Engine));
  return ON1X_RT_OK;
}

static int on1x_rt_value_kind_valid_(On1xRT_ValueKind kind, int allow_void) {
  return (allow_void && kind == ON1X_RT_VALUE_VOID) ||
         kind == ON1X_RT_VALUE_I64 || kind == ON1X_RT_VALUE_BOOL ||
         kind == ON1X_RT_VALUE_F64 || kind == ON1X_RT_VALUE_STRING ||
         kind == ON1X_RT_VALUE_BUFFER;
}

static On1xRT_Status
on1x_rt_signature_check_(const On1xRT_Signature *signature) {
  if (!signature || signature->struct_size < 2u * sizeof(uint32_t))
    return ON1X_RT_INVALID_ARGUMENT;
  if (signature->abi_version != ON1X_RT_ABI_VERSION)
    return ON1X_RT_ABI_MISMATCH;
  if (signature->struct_size < ON1X_RT_REQUIRED(On1xRT_Signature, flags) ||
      signature->parameter_count > ON1X_RT_MAX_PARAMETERS ||
      (!signature->parameters && signature->parameter_count) ||
      !on1x_rt_value_kind_valid_(signature->result, 1) || signature->flags)
    return ON1X_RT_INVALID_ARGUMENT;
  for (size_t index = 0; index < signature->parameter_count; ++index)
    if (!on1x_rt_value_kind_valid_(signature->parameters[index], 0))
      return ON1X_RT_INVALID_ARGUMENT;
  return ON1X_RT_OK;
}

int on1x_rt_signature_equal(const On1xRT_Signature *left,
                            const On1xRT_Signature *right) {
  if (on1x_rt_signature_check_(left) != ON1X_RT_OK ||
      on1x_rt_signature_check_(right) != ON1X_RT_OK ||
      left->parameter_count != right->parameter_count ||
      left->result != right->result)
    return 0;
  return !left->parameter_count ||
         !memcmp(left->parameters, right->parameters,
                 left->parameter_count * sizeof(On1xRT_ValueKind));
}

static On1xRT_Value on1x_rt_value_(On1xRT_ValueKind kind) {
  On1xRT_Value value;
  memset(&value, 0, sizeof(value));
  value.abi_version = ON1X_RT_ABI_VERSION;
  value.struct_size = sizeof(value);
  value.kind = kind;
  return value;
}

On1xRT_Value on1x_rt_value_void(void) {
  return on1x_rt_value_(ON1X_RT_VALUE_VOID);
}

On1xRT_Value on1x_rt_value_i64(int64_t integer) {
  On1xRT_Value value = on1x_rt_value_(ON1X_RT_VALUE_I64);
  value.as.i64 = integer;
  return value;
}

On1xRT_Value on1x_rt_value_bool(int boolean) {
  On1xRT_Value value = on1x_rt_value_(ON1X_RT_VALUE_BOOL);
  value.as.boolean = boolean ? 1u : 0u;
  return value;
}

On1xRT_Value on1x_rt_value_f64(double floating) {
  On1xRT_Value value = on1x_rt_value_(ON1X_RT_VALUE_F64);
  value.as.f64 = floating;
  return value;
}

On1xRT_Value on1x_rt_value_string(On1xRT_String string) {
  On1xRT_Value value = on1x_rt_value_(ON1X_RT_VALUE_STRING);
  value.as.string = string;
  return value;
}

On1xRT_Value on1x_rt_value_buffer(On1xRT_Buffer buffer) {
  On1xRT_Value value = on1x_rt_value_(ON1X_RT_VALUE_BUFFER);
  value.as.buffer = buffer;
  return value;
}

static On1xRT_Status on1x_rt_value_check_(On1xRT_Engine *engine,
                                          const On1xRT_Value *value,
                                          On1xRT_ValueKind expected,
                                          On1xRT_Slot **slot) {
  On1xRT_Status status;
  if (slot)
    *slot = NULL;
  if (!value ||
      !on1x_rt_prefix_(value, value->struct_size,
                       ON1X_RT_REQUIRED(On1xRT_Value, as)) ||
      value->flags || !on1x_rt_value_kind_valid_(value->kind, 1) ||
      (expected != value->kind))
    return ON1X_RT_SIGNATURE_MISMATCH;
  if (value->kind == ON1X_RT_VALUE_BOOL && value->as.boolean > 1u)
    return ON1X_RT_INVALID_ARGUMENT;
  if (value->kind == ON1X_RT_VALUE_STRING) {
    status =
        on1x_rt_slot_get_(engine, value->as.string.id, value->as.string.owner,
                          ON1X_RT_SLOT_STRING, slot, NULL);
    return status;
  }
  if (value->kind == ON1X_RT_VALUE_BUFFER) {
    status =
        on1x_rt_slot_get_(engine, value->as.buffer.id, value->as.buffer.owner,
                          ON1X_RT_SLOT_BUFFER, slot, NULL);
    return status;
  }
  return ON1X_RT_OK;
}

static void on1x_rt_signature_release_(On1xRT_Engine *engine,
                                       On1xRT_Signature *signature) {
  if (signature->parameters)
    on1x_rt_release_(&engine->allocator, (void *)signature->parameters,
                     signature->parameter_count * sizeof(On1xRT_ValueKind),
                     alignof(On1xRT_ValueKind));
  memset(signature, 0, sizeof(*signature));
}

static On1xRT_Status on1x_rt_signature_copy_(On1xRT_Engine *engine,
                                             const On1xRT_Signature *source,
                                             On1xRT_Signature *destination) {
  On1xRT_ValueKind *parameters = NULL;
  const On1xRT_Status status = on1x_rt_signature_check_(source);
  if (status != ON1X_RT_OK)
    return status;
  if (source->parameter_count) {
    const size_t bytes = source->parameter_count * sizeof(On1xRT_ValueKind);
    parameters = (On1xRT_ValueKind *)on1x_rt_allocate_(
        &engine->allocator, bytes, alignof(On1xRT_ValueKind));
    if (!parameters)
      return ON1X_RT_OUT_OF_MEMORY;
    memcpy(parameters, source->parameters, bytes);
  }
  memset(destination, 0, sizeof(*destination));
  destination->abi_version = ON1X_RT_ABI_VERSION;
  destination->struct_size = sizeof(*destination);
  destination->parameters = parameters;
  destination->parameter_count = source->parameter_count;
  destination->result = source->result;
  return ON1X_RT_OK;
}

static On1xRT_Status
on1x_rt_descriptor_validate_(const On1xRT_ModuleDescriptor *descriptor,
                             const char **message) {
  uint32_t flags;
  if (message)
    *message = "invalid module descriptor";
  if (!descriptor ||
      !on1x_rt_prefix_(descriptor, descriptor->struct_size,
                       ON1X_RT_REQUIRED(On1xRT_ModuleDescriptor, export_count)))
    return descriptor && descriptor->abi_version != ON1X_RT_ABI_VERSION
               ? ON1X_RT_ABI_MISMATCH
               : ON1X_RT_INVALID_ARGUMENT;
  if (descriptor->descriptor_version != ON1X_RT_MODULE_DESCRIPTOR_VERSION) {
    if (message)
      *message = "unsupported module descriptor version";
    return ON1X_RT_ABI_MISMATCH;
  }
  flags = descriptor->flags;
  if ((flags != ON1X_RT_MODULE_STATIC &&
       flags != ON1X_RT_MODULE_AUTHORIZED_AOT) ||
      !on1x_rt_name_valid_(descriptor->name) ||
      !on1x_rt_view_valid_(descriptor->revision, ON1X_RT_MAX_NAME_BYTES, 0,
                           0) ||
      !on1x_rt_sha256_valid_(descriptor->identity_sha256) ||
      descriptor->dependency_count > ON1X_RT_MAX_DEPENDENCIES ||
      (!descriptor->dependencies && descriptor->dependency_count) ||
      !descriptor->export_count ||
      descriptor->export_count > ON1X_RT_MAX_EXPORTS || !descriptor->exports) {
    return ON1X_RT_INVALID_ARGUMENT;
  }
  {
    const On1xRT_ModuleInitializeFn initialize =
        ON1X_RT_HAS(descriptor, On1xRT_ModuleDescriptor, initialize)
            ? descriptor->initialize
            : NULL;
    const On1xRT_ModuleShutdownFn shutdown =
        ON1X_RT_HAS(descriptor, On1xRT_ModuleDescriptor, shutdown)
            ? descriptor->shutdown
            : NULL;
    const On1xRT_InstanceCreateFn create =
        ON1X_RT_HAS(descriptor, On1xRT_ModuleDescriptor, instance_create)
            ? descriptor->instance_create
            : NULL;
    const On1xRT_InstanceDestroyFn destroy =
        ON1X_RT_HAS(descriptor, On1xRT_ModuleDescriptor, instance_destroy)
            ? descriptor->instance_destroy
            : NULL;
    if (!!initialize != !!shutdown) {
      if (message)
        *message = "module lifecycle hooks must be paired";
      return ON1X_RT_INVALID_ARGUMENT;
    }
    if (!!create != !!destroy) {
      if (message)
        *message = "instance lifecycle hooks must be paired";
      return ON1X_RT_INVALID_ARGUMENT;
    }
  }
  for (size_t index = 0; index < descriptor->dependency_count; ++index) {
    if (!on1x_rt_name_valid_(descriptor->dependencies[index]) ||
        on1x_rt_view_equal_(descriptor->dependencies[index],
                            descriptor->name)) {
      if (message)
        *message = "invalid or self module dependency";
      return ON1X_RT_INVALID_ARGUMENT;
    }
    for (size_t prior = 0; prior < index; ++prior)
      if (on1x_rt_view_equal_(descriptor->dependencies[index],
                              descriptor->dependencies[prior])) {
        if (message)
          *message = "duplicate module dependency";
        return ON1X_RT_INVALID_ARGUMENT;
      }
  }
  for (size_t index = 0; index < descriptor->export_count; ++index) {
    const On1xRT_ExportDescriptor *exported = descriptor->exports + index;
    On1xRT_Status signature_status;
    if (exported->struct_size < 2u * sizeof(uint32_t)) {
      if (message)
        *message = "short module export descriptor";
      return ON1X_RT_INVALID_ARGUMENT;
    }
    if (exported->abi_version != ON1X_RT_ABI_VERSION) {
      if (message)
        *message = "unsupported module export ABI";
      return ON1X_RT_ABI_MISMATCH;
    }
    if (exported->struct_size <
        ON1X_RT_REQUIRED(On1xRT_ExportDescriptor, invoke)) {
      if (message)
        *message = "short module export descriptor";
      return ON1X_RT_INVALID_ARGUMENT;
    }
    signature_status = on1x_rt_signature_check_(&exported->signature);
    if (!on1x_rt_name_valid_(exported->name) || !exported->invoke ||
        (exported->struct_size >=
             ON1X_RT_REQUIRED(On1xRT_ExportDescriptor, flags) &&
         exported->flags) ||
        (exported->struct_size >=
             ON1X_RT_REQUIRED(On1xRT_ExportDescriptor, reserved) &&
         exported->reserved)) {
      if (message)
        *message = "invalid module export descriptor";
      return ON1X_RT_INVALID_ARGUMENT;
    }
    if (signature_status != ON1X_RT_OK) {
      if (message)
        *message = "invalid module export signature ABI";
      return signature_status;
    }
    for (size_t prior = 0; prior < index; ++prior)
      if (on1x_rt_view_equal_(exported->name,
                              descriptor->exports[prior].name)) {
        if (message)
          *message = "duplicate module export";
        return ON1X_RT_INVALID_ARGUMENT;
      }
  }
  return ON1X_RT_OK;
}

On1xRT_Status
on1x_rt_module_descriptor_check(const On1xRT_ModuleDescriptor *descriptor,
                                On1xRT_Error **error) {
  const char *message = "invalid module descriptor";
  On1xRT_Status status;
  if (!on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_descriptor_validate_(descriptor, &message);
  if (status != ON1X_RT_OK)
    return on1x_rt_default_fail_(error, status, message);
  return ON1X_RT_OK;
}

static char *on1x_rt_copy_text_(On1xRT_Engine *engine, On1xRT_StringView view) {
  char *copy;
  if (view.size == SIZE_MAX)
    return NULL;
  copy = (char *)on1x_rt_allocate_(&engine->allocator, view.size + 1u,
                                   alignof(char));
  if (!copy)
    return NULL;
  if (view.size)
    memcpy(copy, view.data, view.size);
  copy[view.size] = '\0';
  return copy;
}

static void on1x_rt_module_object_release_(On1xRT_Engine *engine,
                                           On1xRT_ModuleObject *module) {
  if (!module)
    return;
  if (module->exports) {
    for (size_t index = 0; index < module->descriptor.export_count; ++index) {
      On1xRT_ExportDescriptor *exported = module->exports + index;
      on1x_rt_signature_release_(engine, &exported->signature);
      if (exported->name.data)
        on1x_rt_release_(&engine->allocator, (void *)exported->name.data,
                         exported->name.size + 1u, alignof(char));
    }
  }
  if (module->exports)
    on1x_rt_release_(&engine->allocator, module->exports,
                     module->descriptor.export_count *
                         sizeof(On1xRT_ExportDescriptor),
                     alignof(On1xRT_ExportDescriptor));
  if (module->dependencies) {
    for (size_t index = 0; index < module->descriptor.dependency_count; ++index)
      if (module->dependencies[index].data)
        on1x_rt_release_(&engine->allocator,
                         (void *)module->dependencies[index].data,
                         module->dependencies[index].size + 1u, alignof(char));
  }
  if (module->dependencies)
    on1x_rt_release_(&engine->allocator, module->dependencies,
                     module->descriptor.dependency_count *
                         sizeof(On1xRT_StringView),
                     alignof(On1xRT_StringView));
  if (module->name)
    on1x_rt_release_(&engine->allocator, module->name,
                     module->descriptor.name.size + 1u, alignof(char));
  if (module->revision)
    on1x_rt_release_(&engine->allocator, module->revision,
                     module->descriptor.revision.size + 1u, alignof(char));
  if (module->identity)
    on1x_rt_release_(&engine->allocator, module->identity,
                     module->descriptor.identity_sha256.size + 1u,
                     alignof(char));
  on1x_rt_release_(&engine->allocator, module, sizeof(*module),
                   alignof(On1xRT_ModuleObject));
}

static On1xRT_Status
on1x_rt_module_object_copy_(On1xRT_Engine *engine,
                            const On1xRT_ModuleDescriptor *source,
                            On1xRT_ModuleObject **destination) {
  On1xRT_ModuleObject *module;
  *destination = NULL;
  module = (On1xRT_ModuleObject *)on1x_rt_allocate_(
      &engine->allocator, sizeof(*module), alignof(On1xRT_ModuleObject));
  if (!module)
    return ON1X_RT_OUT_OF_MEMORY;
  memset(module, 0, sizeof(*module));
  module->descriptor.abi_version = ON1X_RT_ABI_VERSION;
  module->descriptor.struct_size = sizeof(module->descriptor);
  module->descriptor.descriptor_version = source->descriptor_version;
  module->descriptor.flags = source->flags;
  module->descriptor.name = (On1xRT_StringView){NULL, source->name.size};
  module->descriptor.revision =
      (On1xRT_StringView){NULL, source->revision.size};
  module->descriptor.identity_sha256 =
      (On1xRT_StringView){NULL, source->identity_sha256.size};
  module->descriptor.dependency_count = source->dependency_count;
  module->descriptor.export_count = source->export_count;
  module->name = on1x_rt_copy_text_(engine, source->name);
  module->revision = on1x_rt_copy_text_(engine, source->revision);
  module->identity = on1x_rt_copy_text_(engine, source->identity_sha256);
  module->descriptor.name =
      (On1xRT_StringView){module->name, source->name.size};
  module->descriptor.revision =
      (On1xRT_StringView){module->revision, source->revision.size};
  module->descriptor.identity_sha256 =
      (On1xRT_StringView){module->identity, source->identity_sha256.size};
  module->descriptor.provider_context =
      ON1X_RT_HAS(source, On1xRT_ModuleDescriptor, provider_context)
          ? source->provider_context
          : NULL;
  module->descriptor.initialize =
      ON1X_RT_HAS(source, On1xRT_ModuleDescriptor, initialize)
          ? source->initialize
          : NULL;
  module->descriptor.shutdown =
      ON1X_RT_HAS(source, On1xRT_ModuleDescriptor, shutdown) ? source->shutdown
                                                             : NULL;
  module->descriptor.instance_create =
      ON1X_RT_HAS(source, On1xRT_ModuleDescriptor, instance_create)
          ? source->instance_create
          : NULL;
  module->descriptor.instance_destroy =
      ON1X_RT_HAS(source, On1xRT_ModuleDescriptor, instance_destroy)
          ? source->instance_destroy
          : NULL;
  if (!module->name || !module->revision || !module->identity)
    goto allocation_failed;
  if (source->dependency_count) {
    module->dependencies = (On1xRT_StringView *)on1x_rt_allocate_(
        &engine->allocator,
        source->dependency_count * sizeof(On1xRT_StringView),
        alignof(On1xRT_StringView));
    if (!module->dependencies)
      goto allocation_failed;
    memset(module->dependencies, 0,
           source->dependency_count * sizeof(On1xRT_StringView));
    for (size_t index = 0; index < source->dependency_count; ++index) {
      char *copy = on1x_rt_copy_text_(engine, source->dependencies[index]);
      if (!copy)
        goto allocation_failed;
      module->dependencies[index] =
          (On1xRT_StringView){copy, source->dependencies[index].size};
    }
  }
  module->descriptor.dependencies = module->dependencies;
  module->exports = (On1xRT_ExportDescriptor *)on1x_rt_allocate_(
      &engine->allocator,
      source->export_count * sizeof(On1xRT_ExportDescriptor),
      alignof(On1xRT_ExportDescriptor));
  if (!module->exports)
    goto allocation_failed;
  memset(module->exports, 0,
         source->export_count * sizeof(On1xRT_ExportDescriptor));
  for (size_t index = 0; index < source->export_count; ++index) {
    const On1xRT_ExportDescriptor *from = source->exports + index;
    On1xRT_ExportDescriptor *to = module->exports + index;
    char *name = on1x_rt_copy_text_(engine, from->name);
    if (!name)
      goto allocation_failed;
    to->abi_version = ON1X_RT_ABI_VERSION;
    to->struct_size = sizeof(*to);
    to->name = (On1xRT_StringView){name, from->name.size};
    if (on1x_rt_signature_copy_(engine, &from->signature, &to->signature) !=
        ON1X_RT_OK)
      goto allocation_failed;
    to->invoke = from->invoke;
    to->function_context =
        ON1X_RT_HAS(from, On1xRT_ExportDescriptor, function_context)
            ? from->function_context
            : NULL;
  }
  module->descriptor.exports = module->exports;
  *destination = module;
  return ON1X_RT_OK;

allocation_failed:
  on1x_rt_module_object_release_(engine, module);
  return ON1X_RT_OUT_OF_MEMORY;
}

static On1xRT_ModuleObject *on1x_rt_find_module_(On1xRT_Engine *engine,
                                                 On1xRT_StringView name,
                                                 uint32_t *slot_index) {
  for (uint32_t index = 0; index < engine->max_handles; ++index) {
    const On1xRT_Slot *slot = engine->slots + index;
    if (slot->kind == ON1X_RT_SLOT_MODULE) {
      On1xRT_ModuleObject *module = (On1xRT_ModuleObject *)slot->object;
      if (on1x_rt_view_equal_(module->descriptor.name, name)) {
        if (slot_index)
          *slot_index = index;
        return module;
      }
    }
  }
  return NULL;
}

static void on1x_rt_transfer_callback_error_(On1xRT_Error **destination,
                                             On1xRT_Error **source) {
  if (!source || !*source)
    return;
  if (destination && !*destination) {
    *destination = *source;
    *source = NULL;
  } else {
    (void)on1x_rt_error_release(source);
  }
}

static On1xRT_Status on1x_rt_module_initialize_(On1xRT_Engine *engine,
                                                On1xRT_ModuleObject *module,
                                                On1xRT_Error **callback_error) {
  On1xRT_ModuleContext context;
  On1xRT_Status status;
  void *state = NULL;
  if (!module->descriptor.initialize) {
    module->initialized = 1;
    return ON1X_RT_OK;
  }
  memset(&context, 0, sizeof(context));
  context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
  context.engine = engine;
  context.module = module;
  context.active = 1u;
  ++engine->callback_depth;
  status = on1x_rt_status_normalize_(module->descriptor.initialize(
      &context, module->descriptor.provider_context, &state, callback_error));
  --engine->callback_depth;
  context.active = 0u;
  context.magic = 0u;
  if (status == ON1X_RT_OK && callback_error && *callback_error) {
    status = ON1X_RT_CALLBACK_FAILED;
  }
  module->state = state;
  if (status == ON1X_RT_OK)
    module->initialized = 1;
  return status;
}

static On1xRT_Status on1x_rt_module_shutdown_(On1xRT_Engine *engine,
                                              On1xRT_ModuleObject *module,
                                              On1xRT_Error **callback_error) {
  On1xRT_ModuleContext context;
  On1xRT_Status status = ON1X_RT_OK;
  if (!module->initialized)
    return ON1X_RT_OK;
  if (module->descriptor.shutdown) {
    memset(&context, 0, sizeof(context));
    context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
    context.engine = engine;
    context.module = module;
    context.active = 1u;
    ++engine->callback_depth;
    status = on1x_rt_status_normalize_(module->descriptor.shutdown(
        &context, module->descriptor.provider_context, module->state,
        callback_error));
    --engine->callback_depth;
    context.active = 0u;
    context.magic = 0u;
    if (status == ON1X_RT_OK && callback_error && *callback_error)
      status = ON1X_RT_CALLBACK_FAILED;
  }
  if (status == ON1X_RT_OK) {
    module->initialized = 0;
    module->state = NULL;
  }
  return status;
}

static int on1x_rt_staged_index_(On1xRT_ModuleObject *const *staged,
                                 size_t count, On1xRT_StringView name) {
  for (size_t index = 0; index < count; ++index)
    if (on1x_rt_view_equal_(staged[index]->descriptor.name, name))
      return (int)index;
  return -1;
}

static On1xRT_Status on1x_rt_registration_(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *const *descriptors,
    size_t count, On1xRT_Module *modules, size_t module_capacity,
    uint32_t required_flag, On1xRT_Error **error) {
  On1xRT_ModuleObject **staged = NULL;
  size_t *order = NULL;
  uint8_t *selected = NULL;
  uint32_t *publish_slots = NULL;
  size_t initialized_count = 0;
  On1xRT_Error *callback_error = NULL;
  On1xRT_Status status;
  const char *failure_message;
  if (!on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "module registration is unavailable in this context")
               : status;
  if ((!descriptors && count) || (!modules && count) || module_capacity < count)
    return on1x_rt_engine_fail_(
        engine, error, ON1X_RT_INVALID_ARGUMENT,
        "invalid module registration arrays or capacity");
  if (!count)
    return ON1X_RT_OK;
  if (count > ON1X_RT_MAX_MODULES_LIMIT)
    return on1x_rt_engine_fail_(
        engine, error, ON1X_RT_LIMIT,
        "module registration batch exceeds the hard limit");
  for (size_t index = 0; index < count; ++index)
    modules[index] = (On1xRT_Module){0u, 0u};
  for (size_t index = 0; index < count; ++index) {
    const char *message = "invalid module descriptor";
    if (!descriptors[index]) {
      status = ON1X_RT_INVALID_ARGUMENT;
      failure_message = "null module descriptor";
      goto failed;
    }
    status = on1x_rt_descriptor_validate_(descriptors[index], &message);
    if (status != ON1X_RT_OK) {
      failure_message = message;
      goto failed;
    }
    if (descriptors[index]->flags != required_flag) {
      status = ON1X_RT_DENIED;
      failure_message =
          "module trust profile does not match registration operation";
      goto failed;
    }
    if (on1x_rt_find_module_(engine, descriptors[index]->name, NULL)) {
      status = ON1X_RT_INVALID_ARGUMENT;
      failure_message = "module name is already registered";
      goto failed;
    }
    for (size_t prior = 0; prior < index; ++prior)
      if (on1x_rt_view_equal_(descriptors[index]->name,
                              descriptors[prior]->name)) {
        status = ON1X_RT_INVALID_ARGUMENT;
        failure_message = "duplicate module name in transaction";
        goto failed;
      }
  }
  if (count > engine->max_modules - engine->module_count ||
      count > on1x_rt_free_slots_(engine))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_LIMIT,
                                "module or handle registration limit exceeded");
  staged = (On1xRT_ModuleObject **)on1x_rt_allocate_(
      &engine->allocator, count * sizeof(*staged),
      alignof(On1xRT_ModuleObject *));
  if (staged)
    memset(staged, 0, count * sizeof(*staged));
  order = (size_t *)on1x_rt_allocate_(&engine->allocator,
                                      count * sizeof(*order), alignof(size_t));
  selected =
      (uint8_t *)on1x_rt_allocate_(&engine->allocator, count, alignof(uint8_t));
  if (selected)
    memset(selected, 0, count);
  publish_slots = (uint32_t *)on1x_rt_allocate_(
      &engine->allocator, count * sizeof(*publish_slots), alignof(uint32_t));
  if (!staged || !order || !selected || !publish_slots) {
    status = ON1X_RT_OUT_OF_MEMORY;
    failure_message = "cannot allocate module registration transaction";
    goto failed;
  }
  {
    size_t reserved = 0;
    for (uint32_t index = 0; index < engine->max_handles && reserved < count;
         ++index) {
      const On1xRT_Slot *slot = engine->slots + index;
      if (slot->kind == ON1X_RT_SLOT_NONE && !slot->retired)
        publish_slots[reserved++] = index;
    }
    if (reserved != count) {
      status = ON1X_RT_INVALID_STATE;
      failure_message = "handle table changed while reserving module slots";
      goto failed;
    }
  }
  for (size_t index = 0; index < count; ++index) {
    status =
        on1x_rt_module_object_copy_(engine, descriptors[index], staged + index);
    if (status != ON1X_RT_OK) {
      failure_message = "cannot copy module descriptor";
      goto failed;
    }
  }
  for (size_t index = 0; index < count; ++index) {
    On1xRT_ModuleObject *module = staged[index];
    for (size_t dependency = 0;
         dependency < module->descriptor.dependency_count; ++dependency) {
      const On1xRT_StringView name = module->dependencies[dependency];
      if (!on1x_rt_find_module_(engine, name, NULL) &&
          on1x_rt_staged_index_(staged, count, name) < 0) {
        status = ON1X_RT_NOT_FOUND;
        failure_message = "module dependency is not registered";
        goto failed;
      }
    }
  }
  for (size_t position = 0; position < count; ++position) {
    size_t best = count;
    for (size_t candidate = 0; candidate < count; ++candidate) {
      int ready = !selected[candidate];
      On1xRT_ModuleObject *module = staged[candidate];
      if (!ready)
        continue;
      for (size_t dependency = 0;
           dependency < module->descriptor.dependency_count; ++dependency) {
        const int dependency_index = on1x_rt_staged_index_(
            staged, count, module->dependencies[dependency]);
        if (dependency_index >= 0 && !selected[(size_t)dependency_index]) {
          ready = 0;
          break;
        }
      }
      if (ready &&
          (best == count ||
           memcmp(module->name, staged[best]->name,
                  module->descriptor.name.size <
                          staged[best]->descriptor.name.size
                      ? module->descriptor.name.size
                      : staged[best]->descriptor.name.size) < 0 ||
           (module->descriptor.name.size < staged[best]->descriptor.name.size &&
            !memcmp(module->name, staged[best]->name,
                    module->descriptor.name.size)))) {
        best = candidate;
      }
    }
    if (best == count) {
      status = ON1X_RT_INVALID_ARGUMENT;
      failure_message = "cyclic module dependency transaction";
      goto failed;
    }
    selected[best] = 1u;
    order[position] = best;
  }
  if (required_flag == ON1X_RT_MODULE_AUTHORIZED_AOT) {
    if (!engine->authorize_aot) {
      status = ON1X_RT_DENIED;
      failure_message = "AOT registration requires explicit host authorization";
      goto failed;
    }
    for (size_t position = 0; position < count; ++position) {
      const size_t index = order[position];
      ++engine->callback_depth;
      status = on1x_rt_status_normalize_(engine->authorize_aot(
          engine->authorization_context, &staged[index]->descriptor));
      --engine->callback_depth;
      if (status != ON1X_RT_OK) {
        failure_message = "host denied AOT module";
        goto failed;
      }
    }
  }
  for (size_t position = 0; position < count; ++position) {
    const size_t index = order[position];
    callback_error = NULL;
    status = on1x_rt_module_initialize_(engine, staged[index], &callback_error);
    if (status != ON1X_RT_OK) {
      failure_message = "module initialization callback failed";
      if (staged[index]->state && staged[index]->descriptor.shutdown) {
        staged[index]->initialized = 1;
        {
          On1xRT_Error *ignored = NULL;
          (void)on1x_rt_module_shutdown_(engine, staged[index], &ignored);
          if (ignored)
            (void)on1x_rt_error_release(&ignored);
        }
      }
      goto failed;
    }
    ++initialized_count;
  }
  for (size_t index = 0; index < count; ++index) {
    const On1xRT_Slot *slot = engine->slots + publish_slots[index];
    if (slot->kind != ON1X_RT_SLOT_NONE || slot->retired) {
      status = ON1X_RT_INVALID_STATE;
      failure_message = "handle table changed before module commit";
      goto failed;
    }
  }
  for (size_t index = 0; index < count; ++index) {
    const uint32_t slot_index = publish_slots[index];
    On1xRT_Slot *slot = engine->slots + slot_index;
    if (!slot->generation)
      slot->generation = 1u;
    slot->kind = (uint16_t)ON1X_RT_SLOT_MODULE;
    slot->object = staged[index];
    slot->references = 1u;
    ++engine->live_handles;
    modules[index].id = on1x_rt_handle_id_(slot_index, slot->generation);
    modules[index].owner = engine->owner;
    ++engine->module_count;
    staged[index] = NULL;
  }
  on1x_rt_release_(&engine->allocator, publish_slots,
                   count * sizeof(*publish_slots), alignof(uint32_t));
  on1x_rt_release_(&engine->allocator, selected, count, alignof(uint8_t));
  on1x_rt_release_(&engine->allocator, order, count * sizeof(*order),
                   alignof(size_t));
  on1x_rt_release_(&engine->allocator, staged, count * sizeof(*staged),
                   alignof(On1xRT_ModuleObject *));
  return ON1X_RT_OK;

failed:
  if (staged && order) {
    while (initialized_count) {
      const size_t index = order[--initialized_count];
      On1xRT_Error *ignored = NULL;
      (void)on1x_rt_module_shutdown_(engine, staged[index], &ignored);
      if (ignored)
        (void)on1x_rt_error_release(&ignored);
    }
  }
  if (staged) {
    for (size_t index = 0; index < count; ++index)
      on1x_rt_module_object_release_(engine, staged[index]);
  }
  on1x_rt_transfer_callback_error_(error, &callback_error);
  if (publish_slots)
    on1x_rt_release_(&engine->allocator, publish_slots,
                     count * sizeof(*publish_slots), alignof(uint32_t));
  if (selected)
    on1x_rt_release_(&engine->allocator, selected, count, alignof(uint8_t));
  if (order)
    on1x_rt_release_(&engine->allocator, order, count * sizeof(*order),
                     alignof(size_t));
  if (staged)
    on1x_rt_release_(&engine->allocator, staged, count * sizeof(*staged),
                     alignof(On1xRT_ModuleObject *));
  if (error && *error)
    return status;
  return on1x_rt_engine_fail_(engine, error, status, failure_message);
}

On1xRT_Status on1x_rt_engine_register_static_modules(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *const *descriptors,
    size_t count, On1xRT_Module *modules, size_t module_capacity,
    On1xRT_Error **error) {
  return on1x_rt_registration_(engine, descriptors, count, modules,
                               module_capacity, ON1X_RT_MODULE_STATIC, error);
}

On1xRT_Status on1x_rt_engine_register_aot_modules(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *const *descriptors,
    size_t count, On1xRT_Module *modules, size_t module_capacity,
    On1xRT_Error **error) {
  return on1x_rt_registration_(engine, descriptors, count, modules,
                               module_capacity, ON1X_RT_MODULE_AUTHORIZED_AOT,
                               error);
}

On1xRT_Status on1x_rt_engine_register_static_module(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *descriptor,
    On1xRT_Module *module, On1xRT_Error **error) {
  return on1x_rt_engine_register_static_modules(engine, &descriptor, 1u, module,
                                                1u, error);
}

On1xRT_Status on1x_rt_engine_register_aot_module(
    On1xRT_Engine *engine, const On1xRT_ModuleDescriptor *descriptor,
    On1xRT_Module *module, On1xRT_Error **error) {
  return on1x_rt_engine_register_aot_modules(engine, &descriptor, 1u, module,
                                             1u, error);
}

On1xRT_Status on1x_rt_module_unload(On1xRT_Engine *engine,
                                    On1xRT_Module *module,
                                    On1xRT_Error **error) {
  On1xRT_Slot *slot;
  On1xRT_ModuleObject *object;
  On1xRT_Status status;
  uint32_t index;
  On1xRT_Error *callback_error = NULL;
  if (!module || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "module unload is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, module->id, module->owner,
                             ON1X_RT_SLOT_MODULE, &slot, &index);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status, "invalid module handle");
  object = (On1xRT_ModuleObject *)slot->object;
  if (object->instance_count)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_BUSY,
                                "module still owns live instances");
  for (uint32_t other = 0; other < engine->max_handles; ++other) {
    const On1xRT_Slot *candidate = engine->slots + other;
    if (candidate->kind != ON1X_RT_SLOT_MODULE || other == index)
      continue;
    {
      const On1xRT_ModuleObject *dependent =
          (const On1xRT_ModuleObject *)candidate->object;
      for (size_t dependency = 0;
           dependency < dependent->descriptor.dependency_count; ++dependency)
        if (on1x_rt_view_equal_(dependent->dependencies[dependency],
                                object->descriptor.name))
          return on1x_rt_engine_fail_(
              engine, error, ON1X_RT_BUSY,
              "another loaded module depends on this module");
    }
  }
  status = on1x_rt_module_shutdown_(engine, object, &callback_error);
  if (status != ON1X_RT_OK) {
    on1x_rt_transfer_callback_error_(error, &callback_error);
    if (error && *error)
      return status;
    return on1x_rt_engine_fail_(engine, error, status,
                                "module shutdown callback failed");
  }
#ifdef ON1X_RT_HAS_DYNAMIC
  void *native_library = object->native_library;
  int native_fd = object->native_fd;
  size_t native_size = object->native_size;
#endif
  on1x_rt_module_object_release_(engine, object);
  on1x_rt_slot_clear_(engine, index);
  --engine->module_count;
  *module = (On1xRT_Module){0u, 0u};
#ifdef ON1X_RT_HAS_DYNAMIC
  if (native_library) {
    ++engine->callback_depth;
    on1x_rt_dynamic_close_(native_library);
    --engine->callback_depth;
    on1x_rt_dynamic_image_release_(native_fd);
    engine->native_bytes -= native_size;
  }
#endif
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_instance_create(On1xRT_Engine *engine,
                                      On1xRT_Module module,
                                      On1xRT_Instance *instance,
                                      On1xRT_Error **error) {
  On1xRT_Slot *module_slot;
  On1xRT_ModuleObject *module_object;
  On1xRT_InstanceObject *object;
  On1xRT_Status status;
  On1xRT_Error *callback_error = NULL;
  void *instance_state = NULL;
  uint64_t id;
  if (!instance || instance->id || instance->owner ||
      !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "instance creation is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, module.id, module.owner,
                             ON1X_RT_SLOT_MODULE, &module_slot, NULL);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status, "invalid module handle");
  if (!on1x_rt_free_slots_(engine))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_LIMIT,
                                "handle limit exceeded");
  object = (On1xRT_InstanceObject *)on1x_rt_allocate_(
      &engine->allocator, sizeof(*object), alignof(On1xRT_InstanceObject));
  if (!object)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_OUT_OF_MEMORY,
                                "cannot allocate instance");
  memset(object, 0, sizeof(*object));
  module_object = (On1xRT_ModuleObject *)module_slot->object;
  if (module_object->descriptor.instance_create) {
    On1xRT_ModuleContext context;
    memset(&context, 0, sizeof(context));
    context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
    context.engine = engine;
    context.module = module_object;
    context.active = 1u;
    ++engine->callback_depth;
    status =
        on1x_rt_status_normalize_(module_object->descriptor.instance_create(
            &context, module_object->descriptor.provider_context,
            module_object->state, &instance_state, &callback_error));
    --engine->callback_depth;
    context.active = 0u;
    context.magic = 0u;
    if (status == ON1X_RT_OK && callback_error)
      status = ON1X_RT_CALLBACK_FAILED;
    if (status != ON1X_RT_OK) {
      if (instance_state && module_object->descriptor.instance_destroy) {
        On1xRT_Error *ignored = NULL;
        memset(&context, 0, sizeof(context));
        context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
        context.engine = engine;
        context.module = module_object;
        context.active = 1u;
        ++engine->callback_depth;
        (void)module_object->descriptor.instance_destroy(
            &context, module_object->descriptor.provider_context,
            module_object->state, instance_state, &ignored);
        --engine->callback_depth;
        context.active = 0u;
        context.magic = 0u;
        if (ignored)
          (void)on1x_rt_error_release(&ignored);
      }
      on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                       alignof(On1xRT_InstanceObject));
      on1x_rt_transfer_callback_error_(error, &callback_error);
      if (error && *error)
        return status;
      return on1x_rt_engine_fail_(engine, error, status,
                                  "instance creation callback failed");
    }
  }
  object->module_slot = on1x_rt_handle_slot_(module.id);
  object->module_generation = on1x_rt_handle_generation_(module.id);
  object->state = instance_state;
  status =
      on1x_rt_slot_allocate_(engine, ON1X_RT_SLOT_INSTANCE, object, 1u, &id);
  if (status != ON1X_RT_OK) {
    if (module_object->descriptor.instance_destroy) {
      On1xRT_ModuleContext context;
      On1xRT_Error *ignored = NULL;
      memset(&context, 0, sizeof(context));
      context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
      context.engine = engine;
      context.module = module_object;
      context.active = 1u;
      ++engine->callback_depth;
      (void)module_object->descriptor.instance_destroy(
          &context, module_object->descriptor.provider_context,
          module_object->state, instance_state, &ignored);
      --engine->callback_depth;
      context.active = 0u;
      context.magic = 0u;
      if (ignored)
        (void)on1x_rt_error_release(&ignored);
    }
    on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                     alignof(On1xRT_InstanceObject));
    return on1x_rt_engine_fail_(engine, error, status,
                                "cannot publish instance handle");
  }
  ++module_object->instance_count;
  instance->id = id;
  instance->owner = engine->owner;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_instance_release(On1xRT_Engine *engine,
                                       On1xRT_Instance *instance,
                                       On1xRT_Error **error) {
  On1xRT_Slot *slot;
  On1xRT_InstanceObject *object;
  On1xRT_ModuleObject *module;
  On1xRT_Status status;
  uint32_t index;
  On1xRT_Error *callback_error = NULL;
  if (!instance || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "instance release is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, instance->id, instance->owner,
                             ON1X_RT_SLOT_INSTANCE, &slot, &index);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid instance handle");
  object = (On1xRT_InstanceObject *)slot->object;
  if (object->function_count)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_BUSY,
                                "instance still owns live function handles");
  if (object->module_slot >= engine->max_handles ||
      engine->slots[object->module_slot].kind != ON1X_RT_SLOT_MODULE ||
      engine->slots[object->module_slot].generation !=
          object->module_generation)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "instance module lifecycle is inconsistent");
  module = (On1xRT_ModuleObject *)engine->slots[object->module_slot].object;
  if (module->descriptor.instance_destroy) {
    On1xRT_ModuleContext context;
    memset(&context, 0, sizeof(context));
    context.magic = ON1X_RT_MODULE_CONTEXT_MAGIC;
    context.engine = engine;
    context.module = module;
    context.active = 1u;
    ++engine->callback_depth;
    status = on1x_rt_status_normalize_(module->descriptor.instance_destroy(
        &context, module->descriptor.provider_context, module->state,
        object->state, &callback_error));
    --engine->callback_depth;
    context.active = 0u;
    context.magic = 0u;
    if (status == ON1X_RT_OK && callback_error)
      status = ON1X_RT_CALLBACK_FAILED;
    if (status != ON1X_RT_OK) {
      on1x_rt_transfer_callback_error_(error, &callback_error);
      if (error && *error)
        return status;
      return on1x_rt_engine_fail_(engine, error, status,
                                  "instance destruction callback failed");
    }
  }
  --module->instance_count;
  on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                   alignof(On1xRT_InstanceObject));
  on1x_rt_slot_clear_(engine, index);
  *instance = (On1xRT_Instance){0u, 0u};
  return ON1X_RT_OK;
}

On1xRT_Status
on1x_rt_instance_function(On1xRT_Engine *engine, On1xRT_Instance instance,
                          On1xRT_StringView name,
                          const On1xRT_Signature *expected_signature,
                          On1xRT_Function *function, On1xRT_Error **error) {
  On1xRT_Slot *instance_slot;
  On1xRT_InstanceObject *instance_object;
  On1xRT_ModuleObject *module;
  On1xRT_FunctionObject *object;
  On1xRT_Status status;
  On1xRT_Status signature_status;
  size_t export_index;
  uint64_t id;
  if (!function || function->id || function->owner ||
      !on1x_rt_error_ready_(error) || !on1x_rt_name_valid_(name))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "function lookup is unavailable in this context")
               : status;
  signature_status = on1x_rt_signature_check_(expected_signature);
  if (signature_status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, signature_status,
                                "invalid expected function signature ABI");
  status = on1x_rt_slot_get_(engine, instance.id, instance.owner,
                             ON1X_RT_SLOT_INSTANCE, &instance_slot, NULL);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid instance handle");
  instance_object = (On1xRT_InstanceObject *)instance_slot->object;
  if (instance_object->module_slot >= engine->max_handles ||
      engine->slots[instance_object->module_slot].kind != ON1X_RT_SLOT_MODULE ||
      engine->slots[instance_object->module_slot].generation !=
          instance_object->module_generation)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "instance module lifecycle is inconsistent");
  module =
      (On1xRT_ModuleObject *)engine->slots[instance_object->module_slot].object;
  for (export_index = 0; export_index < module->descriptor.export_count;
       ++export_index)
    if (on1x_rt_view_equal_(module->exports[export_index].name, name))
      break;
  if (export_index == module->descriptor.export_count)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_NOT_FOUND,
                                "module export not found");
  if (!on1x_rt_signature_equal(&module->exports[export_index].signature,
                               expected_signature))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_SIGNATURE_MISMATCH,
                                "module export signature does not match");
  object = (On1xRT_FunctionObject *)on1x_rt_allocate_(
      &engine->allocator, sizeof(*object), alignof(On1xRT_FunctionObject));
  if (!object)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_OUT_OF_MEMORY,
                                "cannot allocate function handle");
  object->instance_slot = on1x_rt_handle_slot_(instance.id);
  object->instance_generation = on1x_rt_handle_generation_(instance.id);
  object->export_index = export_index;
  status =
      on1x_rt_slot_allocate_(engine, ON1X_RT_SLOT_FUNCTION, object, 1u, &id);
  if (status != ON1X_RT_OK) {
    on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                     alignof(On1xRT_FunctionObject));
    return on1x_rt_engine_fail_(engine, error, status, "handle limit exceeded");
  }
  ++instance_object->function_count;
  function->id = id;
  function->owner = engine->owner;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_function_release(On1xRT_Engine *engine,
                                       On1xRT_Function *function,
                                       On1xRT_Error **error) {
  On1xRT_Slot *slot;
  On1xRT_FunctionObject *object;
  On1xRT_Status status;
  uint32_t index;
  if (!function || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "function release is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, function->id, function->owner,
                             ON1X_RT_SLOT_FUNCTION, &slot, &index);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid function handle");
  object = (On1xRT_FunctionObject *)slot->object;
  if (object->instance_slot >= engine->max_handles ||
      engine->slots[object->instance_slot].kind != ON1X_RT_SLOT_INSTANCE ||
      engine->slots[object->instance_slot].generation !=
          object->instance_generation)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "function instance lifecycle is inconsistent");
  --((On1xRT_InstanceObject *)engine->slots[object->instance_slot].object)
        ->function_count;
  on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                   alignof(On1xRT_FunctionObject));
  on1x_rt_slot_clear_(engine, index);
  *function = (On1xRT_Function){0u, 0u};
  return ON1X_RT_OK;
}

static On1xRT_Status on1x_rt_byte_create_(On1xRT_Engine *engine,
                                          const void *bytes, size_t size,
                                          On1xRT_SlotKind kind,
                                          uint64_t pending_call, uint64_t *id) {
  On1xRT_ByteObject *object;
  On1xRT_Status status;
  if (!bytes && size)
    return ON1X_RT_INVALID_ARGUMENT;
  if (size > engine->max_value_bytes ||
      size > engine->max_value_bytes - engine->value_bytes)
    return ON1X_RT_LIMIT;
  if (kind == ON1X_RT_SLOT_STRING && size &&
      !on1x_rt_utf8_((const uint8_t *)bytes, size))
    return ON1X_RT_INVALID_ARGUMENT;
  object = (On1xRT_ByteObject *)on1x_rt_allocate_(
      &engine->allocator, sizeof(*object), alignof(On1xRT_ByteObject));
  if (!object)
    return ON1X_RT_OUT_OF_MEMORY;
  memset(object, 0, sizeof(*object));
  if (size) {
    object->data = (uint8_t *)on1x_rt_allocate_(&engine->allocator, size,
                                                alignof(uint8_t));
    if (!object->data) {
      on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                       alignof(On1xRT_ByteObject));
      return ON1X_RT_OUT_OF_MEMORY;
    }
    memcpy(object->data, bytes, size);
  }
  object->size = size;
  object->pending_call = pending_call;
  status = on1x_rt_slot_allocate_(engine, kind, object, 1u, id);
  if (status != ON1X_RT_OK) {
    on1x_rt_release_(&engine->allocator, object->data, size, alignof(uint8_t));
    on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                     alignof(On1xRT_ByteObject));
    return status;
  }
  engine->value_bytes += size;
  return ON1X_RT_OK;
}

static void on1x_rt_byte_slot_release_(On1xRT_Engine *engine, On1xRT_Slot *slot,
                                       uint32_t index) {
  On1xRT_ByteObject *object;
  if (--slot->references)
    return;
  object = (On1xRT_ByteObject *)slot->object;
  engine->value_bytes -= object->size;
  on1x_rt_release_(&engine->allocator, object->data, object->size,
                   alignof(uint8_t));
  on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                   alignof(On1xRT_ByteObject));
  on1x_rt_slot_clear_(engine, index);
}

static On1xRT_Status on1x_rt_byte_release_handle_(On1xRT_Engine *engine,
                                                  uint64_t id, uint64_t owner,
                                                  On1xRT_SlotKind kind) {
  On1xRT_Slot *slot;
  uint32_t index;
  On1xRT_Status status =
      on1x_rt_slot_get_(engine, id, owner, kind, &slot, &index);
  if (status != ON1X_RT_OK)
    return status;
  on1x_rt_byte_slot_release_(engine, slot, index);
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_string_create(On1xRT_Engine *engine, const void *bytes,
                                    size_t size, On1xRT_String *string,
                                    On1xRT_Error **error) {
  On1xRT_Status status;
  uint64_t id;
  if (!string || string->id || string->owner || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status =
        on1x_rt_byte_create_(engine, bytes, size, ON1X_RT_SLOT_STRING, 0u, &id);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot create UTF-8 string")
               : status;
  string->id = id;
  string->owner = engine->owner;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_buffer_create(On1xRT_Engine *engine, const void *bytes,
                                    size_t size, On1xRT_Buffer *buffer,
                                    On1xRT_Error **error) {
  On1xRT_Status status;
  uint64_t id;
  if (!buffer || buffer->id || buffer->owner || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status =
        on1x_rt_byte_create_(engine, bytes, size, ON1X_RT_SLOT_BUFFER, 0u, &id);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot create byte buffer")
               : status;
  buffer->id = id;
  buffer->owner = engine->owner;
  return ON1X_RT_OK;
}

static On1xRT_Status on1x_rt_byte_view_(On1xRT_Engine *engine, uint64_t id,
                                        uint64_t owner, On1xRT_SlotKind kind,
                                        On1xRT_BytesView *view) {
  On1xRT_Slot *slot;
  On1xRT_Status status;
  On1xRT_ByteObject *object;
  if (!view)
    return ON1X_RT_INVALID_ARGUMENT;
  view->data = NULL;
  view->size = 0u;
  status = on1x_rt_slot_get_(engine, id, owner, kind, &slot, NULL);
  if (status != ON1X_RT_OK)
    return status;
  object = (On1xRT_ByteObject *)slot->object;
  view->data = object->data;
  view->size = object->size;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_string_view(On1xRT_Engine *engine, On1xRT_String string,
                                  On1xRT_BytesView *view,
                                  On1xRT_Error **error) {
  On1xRT_Status status;
  if (!view || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  view->data = NULL;
  view->size = 0u;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_view_(engine, string.id, string.owner,
                                ON1X_RT_SLOT_STRING, view);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot borrow string view")
               : status;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_buffer_view(On1xRT_Engine *engine, On1xRT_Buffer buffer,
                                  On1xRT_BytesView *view,
                                  On1xRT_Error **error) {
  On1xRT_Status status;
  if (!view || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  view->data = NULL;
  view->size = 0u;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_view_(engine, buffer.id, buffer.owner,
                                ON1X_RT_SLOT_BUFFER, view);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot borrow buffer view")
               : status;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_buffer_write(On1xRT_Engine *engine, On1xRT_Buffer buffer,
                                   size_t offset, const void *bytes,
                                   size_t size, On1xRT_Error **error) {
  On1xRT_Slot *slot;
  On1xRT_ByteObject *object;
  On1xRT_Status status;
  if ((!bytes && size) || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status = on1x_rt_slot_get_(engine, buffer.id, buffer.owner,
                               ON1X_RT_SLOT_BUFFER, &slot, NULL);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid buffer handle")
               : status;
  object = (On1xRT_ByteObject *)slot->object;
  if (offset > object->size || size > object->size - offset)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_ARGUMENT,
                                "buffer write is out of bounds");
  if (size)
    memmove(object->data + offset, bytes, size);
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_string_release(On1xRT_Engine *engine,
                                     On1xRT_String *string,
                                     On1xRT_Error **error) {
  On1xRT_Status status;
  if (!string || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_release_handle_(engine, string->id, string->owner,
                                          ON1X_RT_SLOT_STRING);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot release string")
               : status;
  *string = (On1xRT_String){0u, 0u};
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_buffer_release(On1xRT_Engine *engine,
                                     On1xRT_Buffer *buffer,
                                     On1xRT_Error **error) {
  On1xRT_Status status;
  if (!buffer || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_release_handle_(engine, buffer->id, buffer->owner,
                                          ON1X_RT_SLOT_BUFFER);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot release buffer")
               : status;
  *buffer = (On1xRT_Buffer){0u, 0u};
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_value_release(On1xRT_Engine *engine, On1xRT_Value *value,
                                    On1xRT_Error **error) {
  On1xRT_Status status;
  if (!value || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "cannot release value")
               : status;
  if (!on1x_rt_prefix_(value, value->struct_size,
                       ON1X_RT_REQUIRED(On1xRT_Value, as)) ||
      value->flags || !on1x_rt_value_kind_valid_(value->kind, 1))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_ARGUMENT,
                                "invalid value descriptor");
  if (value->kind == ON1X_RT_VALUE_STRING)
    status = on1x_rt_byte_release_handle_(engine, value->as.string.id,
                                          value->as.string.owner,
                                          ON1X_RT_SLOT_STRING);
  else if (value->kind == ON1X_RT_VALUE_BUFFER)
    status = on1x_rt_byte_release_handle_(engine, value->as.buffer.id,
                                          value->as.buffer.owner,
                                          ON1X_RT_SLOT_BUFFER);
  else
    status = ON1X_RT_OK;
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid owned value handle");
  *value = on1x_rt_value_void();
  return ON1X_RT_OK;
}

static void on1x_rt_host_object_release_(On1xRT_Engine *engine,
                                         On1xRT_HostObject *object) {
  if (!object)
    return;
  on1x_rt_signature_release_(engine, &object->signature);
  if (object->name)
    on1x_rt_release_(&engine->allocator, object->name, object->name_size + 1u,
                     alignof(char));
  on1x_rt_release_(&engine->allocator, object, sizeof(*object),
                   alignof(On1xRT_HostObject));
}

On1xRT_Status on1x_rt_engine_register_host_function(
    On1xRT_Engine *engine, const On1xRT_HostFunctionDescriptor *descriptor,
    On1xRT_HostFunction *function, On1xRT_Error **error) {
  On1xRT_HostObject *object;
  On1xRT_Status status;
  On1xRT_Status signature_status;
  uint64_t id;
  if (!function || function->id || function->owner || !descriptor ||
      !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "host registration is unavailable in this context")
               : status;
  if (descriptor->struct_size >= 2u * sizeof(uint32_t) &&
      descriptor->abi_version != ON1X_RT_ABI_VERSION)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_ABI_MISMATCH,
                                "unsupported host function descriptor ABI");
  if (!on1x_rt_prefix_(
          descriptor, descriptor->struct_size,
          ON1X_RT_REQUIRED(On1xRT_HostFunctionDescriptor, invoke)) ||
      !on1x_rt_name_valid_(descriptor->name) || !descriptor->invoke ||
      (descriptor->struct_size >=
           ON1X_RT_REQUIRED(On1xRT_HostFunctionDescriptor, flags) &&
       descriptor->flags) ||
      (descriptor->struct_size >=
           ON1X_RT_REQUIRED(On1xRT_HostFunctionDescriptor, reserved) &&
       descriptor->reserved))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_ARGUMENT,
                                "invalid host function descriptor");
  signature_status = on1x_rt_signature_check_(&descriptor->signature);
  if (signature_status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, signature_status,
                                "invalid host function signature ABI");
  if (engine->host_function_count >= engine->max_host_functions)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_LIMIT,
                                "host function limit exceeded");
  for (uint32_t index = 0; index < engine->max_handles; ++index)
    if (engine->slots[index].kind == ON1X_RT_SLOT_HOST_FUNCTION) {
      const On1xRT_HostObject *existing =
          (const On1xRT_HostObject *)engine->slots[index].object;
      const On1xRT_StringView existing_name = {existing->name,
                                               existing->name_size};
      if (on1x_rt_view_equal_(existing_name, descriptor->name))
        return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_ARGUMENT,
                                    "host function name is already registered");
    }
  object = (On1xRT_HostObject *)on1x_rt_allocate_(
      &engine->allocator, sizeof(*object), alignof(On1xRT_HostObject));
  if (!object)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_OUT_OF_MEMORY,
                                "cannot allocate host function");
  memset(object, 0, sizeof(*object));
  object->name = on1x_rt_copy_text_(engine, descriptor->name);
  object->name_size = descriptor->name.size;
  status = on1x_rt_signature_copy_(engine, &descriptor->signature,
                                   &object->signature);
  if (!object->name || status != ON1X_RT_OK) {
    on1x_rt_host_object_release_(engine, object);
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_OUT_OF_MEMORY,
                                "cannot copy host function descriptor");
  }
  object->invoke = descriptor->invoke;
  object->function_context =
      ON1X_RT_HAS(descriptor, On1xRT_HostFunctionDescriptor, function_context)
          ? descriptor->function_context
          : NULL;
  status = on1x_rt_slot_allocate_(engine, ON1X_RT_SLOT_HOST_FUNCTION, object,
                                  1u, &id);
  if (status != ON1X_RT_OK) {
    on1x_rt_host_object_release_(engine, object);
    return on1x_rt_engine_fail_(engine, error, status, "handle limit exceeded");
  }
  ++engine->host_function_count;
  function->id = id;
  function->owner = engine->owner;
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_host_function_unregister(On1xRT_Engine *engine,
                                               On1xRT_HostFunction *function,
                                               On1xRT_Error **error) {
  On1xRT_Slot *slot;
  On1xRT_HostObject *object;
  On1xRT_Status status;
  uint32_t index;
  if (!function || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "host unregister is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, function->id, function->owner,
                             ON1X_RT_SLOT_HOST_FUNCTION, &slot, &index);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid host function handle");
  object = (On1xRT_HostObject *)slot->object;
  if (object->active)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_BUSY,
                                "host function is active");
  on1x_rt_host_object_release_(engine, object);
  on1x_rt_slot_clear_(engine, index);
  --engine->host_function_count;
  *function = (On1xRT_HostFunction){0u, 0u};
  return ON1X_RT_OK;
}

static On1xRT_Status on1x_rt_call_check_(On1xRT_CallContext *call) {
  if (!call || call->magic != ON1X_RT_CALL_MAGIC || !call->active ||
      !call->engine)
    return ON1X_RT_INVALID_STATE;
  return on1x_rt_engine_check_(call->engine, 1);
}

static On1xRT_Status
on1x_rt_invoke_native_(On1xRT_CallContext *call,
                       const On1xRT_Signature *signature,
                       On1xRT_NativeFunction invoke, void *function_context,
                       const On1xRT_Value *arguments, size_t argument_count,
                       On1xRT_Value *result, On1xRT_Error **error) {
  On1xRT_Error *callback_error = NULL;
  On1xRT_Status status;
  if (!result || !on1x_rt_error_ready_(error) ||
      argument_count != signature->parameter_count ||
      (!arguments && argument_count)) {
    return ON1X_RT_INVALID_ARGUMENT;
  }
  *result = on1x_rt_value_void();
  for (size_t index = 0; index < argument_count; ++index) {
    status = on1x_rt_value_check_(call->engine, arguments + index,
                                  signature->parameters[index], NULL);
    if (status != ON1X_RT_OK)
      return status;
  }
  ++call->engine->callback_depth;
  status = on1x_rt_status_normalize_(invoke(call, function_context, arguments,
                                            argument_count, result,
                                            &callback_error));
  --call->engine->callback_depth;
  if (status == ON1X_RT_OK && callback_error)
    status = ON1X_RT_CALLBACK_FAILED;
  if (status == ON1X_RT_OK)
    status =
        on1x_rt_value_check_(call->engine, result, signature->result, NULL);
  if (status == ON1X_RT_OK && (result->kind == ON1X_RT_VALUE_STRING ||
                               result->kind == ON1X_RT_VALUE_BUFFER)) {
    On1xRT_Slot *slot = NULL;
    On1xRT_ByteObject *object;
    status =
        on1x_rt_value_check_(call->engine, result, signature->result, &slot);
    if (status == ON1X_RT_OK) {
      object = (On1xRT_ByteObject *)slot->object;
      if (object->pending_call != call->call_id)
        status = ON1X_RT_INVALID_STATE;
    }
  }
  if (status != ON1X_RT_OK) {
    on1x_rt_transfer_callback_error_(error, &callback_error);
    *result = on1x_rt_value_void();
    if (error && *error)
      return status;
    return on1x_rt_engine_fail_(
        call->engine, error, status,
        "native callback failed or returned an invalid value");
  }
  return ON1X_RT_OK;
}

static void on1x_rt_cleanup_pending_(On1xRT_CallContext *call,
                                     uint64_t kept_id) {
  On1xRT_Engine *engine = call->engine;
  for (uint32_t index = 0; index < engine->max_handles; ++index) {
    On1xRT_Slot *slot = engine->slots + index;
    if (slot->kind != ON1X_RT_SLOT_STRING && slot->kind != ON1X_RT_SLOT_BUFFER)
      continue;
    {
      On1xRT_ByteObject *object = (On1xRT_ByteObject *)slot->object;
      if (object->pending_call != call->call_id)
        continue;
      if (on1x_rt_handle_id_(index, slot->generation) == kept_id) {
        object->pending_call = 0u;
      } else {
        object->pending_call = 0u;
        on1x_rt_byte_slot_release_(engine, slot, index);
      }
    }
  }
}

On1xRT_Status on1x_rt_function_call(On1xRT_Engine *engine,
                                    On1xRT_Function function,
                                    const On1xRT_Value *arguments,
                                    size_t argument_count, On1xRT_Value *result,
                                    On1xRT_Error **error) {
  On1xRT_Slot *function_slot;
  On1xRT_FunctionObject *function_object;
  On1xRT_InstanceObject *instance;
  On1xRT_ModuleObject *module;
  On1xRT_ExportDescriptor *exported;
  On1xRT_CallContext call;
  On1xRT_Status status;
  uint64_t kept_id = 0u;
  if (!result || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  *result = on1x_rt_value_void();
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(
                     engine, error, status,
                     "function call is unavailable in this context")
               : status;
  status = on1x_rt_slot_get_(engine, function.id, function.owner,
                             ON1X_RT_SLOT_FUNCTION, &function_slot, NULL);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status,
                                "invalid function handle");
  function_object = (On1xRT_FunctionObject *)function_slot->object;
  if (function_object->instance_slot >= engine->max_handles ||
      engine->slots[function_object->instance_slot].kind !=
          ON1X_RT_SLOT_INSTANCE ||
      engine->slots[function_object->instance_slot].generation !=
          function_object->instance_generation)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "function instance lifecycle is inconsistent");
  instance =
      (On1xRT_InstanceObject *)engine->slots[function_object->instance_slot]
          .object;
  if (instance->module_slot >= engine->max_handles ||
      engine->slots[instance->module_slot].kind != ON1X_RT_SLOT_MODULE ||
      engine->slots[instance->module_slot].generation !=
          instance->module_generation)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "function module lifecycle is inconsistent");
  module = (On1xRT_ModuleObject *)engine->slots[instance->module_slot].object;
  if (function_object->export_index >= module->descriptor.export_count)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_STATE,
                                "function export index is invalid");
  exported = module->exports + function_object->export_index;
  memset(&call, 0, sizeof(call));
  call.magic = ON1X_RT_CALL_MAGIC;
  call.engine = engine;
  call.module = module;
  call.instance = instance;
  call.call_id = engine->next_call++;
  if (!call.call_id)
    call.call_id = engine->next_call++;
  call.active = 1u;
  call.expose_module_state = 1u;
  status = on1x_rt_invoke_native_(&call, &exported->signature, exported->invoke,
                                  exported->function_context, arguments,
                                  argument_count, result, error);
  if (status == ON1X_RT_OK) {
    if (result->kind == ON1X_RT_VALUE_STRING)
      kept_id = result->as.string.id;
    else if (result->kind == ON1X_RT_VALUE_BUFFER)
      kept_id = result->as.buffer.id;
  }
  on1x_rt_cleanup_pending_(&call, kept_id);
  call.active = 0u;
  call.magic = 0u;
  if (status != ON1X_RT_OK)
    *result = on1x_rt_value_void();
  return status;
}

On1xRT_Status on1x_rt_call_string_view(On1xRT_CallContext *call,
                                       On1xRT_String string,
                                       On1xRT_BytesView *view,
                                       On1xRT_Error **error) {
  On1xRT_Status status;
  if (!view || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  view->data = NULL;
  view->size = 0u;
  status = on1x_rt_call_check_(call);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_view_(call->engine, string.id, string.owner,
                                ON1X_RT_SLOT_STRING, view);
  if (status != ON1X_RT_OK && call && call->engine)
    return on1x_rt_engine_fail_(call->engine, error, status,
                                "cannot borrow callback string");
  return status;
}

On1xRT_Status on1x_rt_call_buffer_view(On1xRT_CallContext *call,
                                       On1xRT_Buffer buffer,
                                       On1xRT_BytesView *view,
                                       On1xRT_Error **error) {
  On1xRT_Status status;
  if (!view || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  view->data = NULL;
  view->size = 0u;
  status = on1x_rt_call_check_(call);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_view_(call->engine, buffer.id, buffer.owner,
                                ON1X_RT_SLOT_BUFFER, view);
  if (status != ON1X_RT_OK && call && call->engine)
    return on1x_rt_engine_fail_(call->engine, error, status,
                                "cannot borrow callback buffer");
  return status;
}

On1xRT_Status on1x_rt_call_string_create(On1xRT_CallContext *call,
                                         const void *bytes, size_t size,
                                         On1xRT_String *string,
                                         On1xRT_Error **error) {
  On1xRT_Status status;
  uint64_t id;
  if (!string || string->id || string->owner || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_check_(call);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_create_(call->engine, bytes, size,
                                  ON1X_RT_SLOT_STRING, call->call_id, &id);
  if (status != ON1X_RT_OK && call && call->engine)
    return on1x_rt_engine_fail_(call->engine, error, status,
                                "cannot create callback string");
  if (status == ON1X_RT_OK) {
    string->id = id;
    string->owner = call->engine->owner;
  }
  return status;
}

On1xRT_Status on1x_rt_call_buffer_create(On1xRT_CallContext *call,
                                         const void *bytes, size_t size,
                                         On1xRT_Buffer *buffer,
                                         On1xRT_Error **error) {
  On1xRT_Status status;
  uint64_t id;
  if (!buffer || buffer->id || buffer->owner || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_check_(call);
  if (status == ON1X_RT_OK)
    status = on1x_rt_byte_create_(call->engine, bytes, size,
                                  ON1X_RT_SLOT_BUFFER, call->call_id, &id);
  if (status != ON1X_RT_OK && call && call->engine)
    return on1x_rt_engine_fail_(call->engine, error, status,
                                "cannot create callback buffer");
  if (status == ON1X_RT_OK) {
    buffer->id = id;
    buffer->owner = call->engine->owner;
  }
  return status;
}

On1xRT_Status on1x_rt_call_value_release(On1xRT_CallContext *call,
                                         On1xRT_Value *value,
                                         On1xRT_Error **error) {
  On1xRT_Status status;
  On1xRT_Slot *slot;
  uint32_t index;
  uint64_t id;
  uint64_t owner;
  On1xRT_SlotKind kind;
  if (!value || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_check_(call);
  if (status != ON1X_RT_OK)
    return call && call->engine
               ? on1x_rt_engine_fail_(call->engine, error, status,
                                      "invalid call context")
               : status;
  if (!on1x_rt_prefix_(value, value->struct_size,
                       ON1X_RT_REQUIRED(On1xRT_Value, as)) ||
      value->flags || !on1x_rt_value_kind_valid_(value->kind, 1))
    return on1x_rt_engine_fail_(call->engine, error, ON1X_RT_INVALID_ARGUMENT,
                                "invalid callback value");
  if (value->kind != ON1X_RT_VALUE_STRING &&
      value->kind != ON1X_RT_VALUE_BUFFER) {
    *value = on1x_rt_value_void();
    return ON1X_RT_OK;
  }
  if (value->kind == ON1X_RT_VALUE_STRING) {
    id = value->as.string.id;
    owner = value->as.string.owner;
    kind = ON1X_RT_SLOT_STRING;
  } else {
    id = value->as.buffer.id;
    owner = value->as.buffer.owner;
    kind = ON1X_RT_SLOT_BUFFER;
  }
  status = on1x_rt_slot_get_(call->engine, id, owner, kind, &slot, &index);
  if (status == ON1X_RT_OK &&
      ((On1xRT_ByteObject *)slot->object)->pending_call != call->call_id)
    status = ON1X_RT_INVALID_STATE;
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(
        call->engine, error, status,
        "callback may release only values created in this call");
  ((On1xRT_ByteObject *)slot->object)->pending_call = 0u;
  on1x_rt_byte_slot_release_(call->engine, slot, index);
  *value = on1x_rt_value_void();
  return ON1X_RT_OK;
}

On1xRT_Status on1x_rt_call_host(On1xRT_CallContext *call,
                                On1xRT_StringView name,
                                const On1xRT_Signature *signature,
                                const On1xRT_Value *arguments,
                                size_t argument_count, On1xRT_Value *result,
                                On1xRT_Error **error) {
  On1xRT_Status status;
  On1xRT_Status signature_status;
  On1xRT_HostObject *object = NULL;
  unsigned expose;
  if (!result || !on1x_rt_error_ready_(error) || !on1x_rt_name_valid_(name))
    return ON1X_RT_INVALID_ARGUMENT;
  *result = on1x_rt_value_void();
  status = on1x_rt_call_check_(call);
  if (status != ON1X_RT_OK)
    return call && call->engine
               ? on1x_rt_engine_fail_(call->engine, error, status,
                                      "invalid call context")
               : status;
  signature_status = on1x_rt_signature_check_(signature);
  if (signature_status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(call->engine, error, signature_status,
                                "invalid callback host signature ABI");
  for (uint32_t index = 0; index < call->engine->max_handles; ++index) {
    const On1xRT_Slot *slot = call->engine->slots + index;
    if (slot->kind == ON1X_RT_SLOT_HOST_FUNCTION) {
      On1xRT_HostObject *candidate = (On1xRT_HostObject *)slot->object;
      const On1xRT_StringView candidate_name = {candidate->name,
                                                candidate->name_size};
      if (on1x_rt_view_equal_(candidate_name, name)) {
        object = candidate;
        break;
      }
    }
  }
  if (!object)
    return on1x_rt_engine_fail_(call->engine, error, ON1X_RT_NOT_FOUND,
                                "host function not found");
  if (!on1x_rt_signature_equal(&object->signature, signature))
    return on1x_rt_engine_fail_(call->engine, error, ON1X_RT_SIGNATURE_MISMATCH,
                                "host function signature does not match");
  if (object->active)
    return on1x_rt_engine_fail_(
        call->engine, error, ON1X_RT_REENTRANT,
        "recursive host function invocation is forbidden");
  object->active = 1u;
  expose = call->expose_module_state;
  call->expose_module_state = 0u;
  status = on1x_rt_invoke_native_(call, &object->signature, object->invoke,
                                  object->function_context, arguments,
                                  argument_count, result, error);
  call->expose_module_state = expose;
  object->active = 0u;
  return status;
}

On1xRT_Status on1x_rt_call_error(On1xRT_CallContext *call, uint32_t code,
                                 On1xRT_StringView message,
                                 On1xRT_Error **error) {
  On1xRT_Status status;
  if (!error || *error || !code ||
      !on1x_rt_view_valid_(message, ON1X_RT_MAX_ERROR_BYTES, 0, 1))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_check_(call);
  if (status != ON1X_RT_OK)
    return status;
  *error = on1x_rt_make_error_(&call->engine->allocator, code, message.data,
                               message.size);
  return *error ? ON1X_RT_OK : ON1X_RT_OUT_OF_MEMORY;
}

void *on1x_rt_call_module_state(On1xRT_CallContext *call) {
  if (on1x_rt_call_check_(call) != ON1X_RT_OK || !call->expose_module_state ||
      !call->module)
    return NULL;
  return call->module->state;
}

void *on1x_rt_call_instance_state(On1xRT_CallContext *call) {
  if (on1x_rt_call_check_(call) != ON1X_RT_OK || !call->expose_module_state ||
      !call->instance)
    return NULL;
  return call->instance->state;
}

static On1xRT_Status
on1x_rt_module_context_check_(On1xRT_ModuleContext *context) {
  if (!context || context->magic != ON1X_RT_MODULE_CONTEXT_MAGIC ||
      !context->active || !context->engine)
    return ON1X_RT_INVALID_STATE;
  return on1x_rt_engine_check_(context->engine, 1);
}

void *on1x_rt_module_allocate(On1xRT_ModuleContext *context, size_t size,
                              size_t alignment) {
  if (on1x_rt_module_context_check_(context) != ON1X_RT_OK || !size ||
      size > context->engine->max_value_bytes ||
      !on1x_rt_alignment_valid_(alignment))
    return NULL;
  return on1x_rt_allocate_(&context->engine->allocator, size, alignment);
}

void on1x_rt_module_release_memory(On1xRT_ModuleContext *context, void *pointer,
                                   size_t size, size_t alignment) {
  if (on1x_rt_module_context_check_(context) != ON1X_RT_OK || !pointer ||
      !size || size > context->engine->max_value_bytes ||
      !on1x_rt_alignment_valid_(alignment))
    return;
  on1x_rt_release_(&context->engine->allocator, pointer, size, alignment);
}

On1xRT_Status on1x_rt_module_error(On1xRT_ModuleContext *context, uint32_t code,
                                   On1xRT_StringView message,
                                   On1xRT_Error **error) {
  On1xRT_Status status;
  if (!error || *error || !code ||
      !on1x_rt_view_valid_(message, ON1X_RT_MAX_ERROR_BYTES, 0, 1))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_module_context_check_(context);
  if (status != ON1X_RT_OK)
    return status;
  *error = on1x_rt_make_error_(&context->engine->allocator, code, message.data,
                               message.size);
  return *error ? ON1X_RT_OK : ON1X_RT_OUT_OF_MEMORY;
}

On1xRT_Status on1x_rt_feature_status(On1xRT_Feature feature) {
  if (feature < ON1X_RT_FEATURE_DYNAMIC_MODULE ||
      feature > ON1X_RT_FEATURE_PROFILER)
    return ON1X_RT_INVALID_ARGUMENT;
#ifdef ON1X_RT_HAS_DYNAMIC
  if (feature == ON1X_RT_FEATURE_DYNAMIC_MODULE)
    return ON1X_RT_OK;
#endif
  return ON1X_RT_UNSUPPORTED;
}

On1xRT_Status on1x_rt_engine_load_dynamic(On1xRT_Engine *engine,
                                          On1xRT_StringView path,
                                          On1xRT_Module *module,
                                          On1xRT_Error **error) {
  On1xRT_Status status;
  if (!module || module->id || module->owner || !on1x_rt_error_ready_(error) ||
      !on1x_rt_view_valid_(path, ON1X_RT_MAX_DYNAMIC_PATH_BYTES, 0, 0))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid engine context")
               : status;
  return on1x_rt_engine_fail_(engine, error, ON1X_RT_UNSUPPORTED,
                               "path-only dynamic loading is unsupported");
}

On1xRT_Status on1x_rt_engine_load_authorized_dynamic(
    On1xRT_Engine *engine, On1xRT_StringView path,
    On1xRT_AuthorizeDynamicFn authorize, void *authorization_context,
    On1xRT_Module *module, On1xRT_Error **error) {
  On1xRT_Status status;
  if (!module || module->id || module->owner || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid engine context")
               : status;
  if (!on1x_rt_view_valid_(path, ON1X_RT_MAX_DYNAMIC_PATH_BYTES, 0, 0) ||
      path.data[0] != '/')
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_INVALID_ARGUMENT,
                               "native module path must be absolute UTF-8");
#ifndef ON1X_RT_HAS_DYNAMIC
  (void)authorize;
  (void)authorization_context;
  return on1x_rt_engine_fail_(engine, error, ON1X_RT_UNSUPPORTED,
                               "trusted dynamic native loading is unavailable");
#else
  char request_path[ON1X_RT_MAX_DYNAMIC_PATH_BYTES + 1u];
  On1xRT_DynamicImage image = {.fd = -1};
  void *library = NULL;
  On1xRT_ModuleEntryFn entry = NULL;
  On1xRT_Module published = {0u, 0u};
  const On1xRT_ModuleDescriptor *descriptor = NULL;
  const char *message = "invalid native module image";
  const size_t remaining = ON1X_RT_MAX_DYNAMIC_ENGINE_BYTES - engine->native_bytes;
  if (!authorize || !engine->authorize_aot)
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_DENIED,
                               "native loading requires both host policies");
  if (engine->module_count >= engine->max_modules || !on1x_rt_free_slots_(engine))
    return on1x_rt_engine_fail_(engine, error, ON1X_RT_LIMIT,
                               "module or handle registration limit exceeded");
  memcpy(request_path, path.data, path.size);
  request_path[path.size] = '\0';
  status = on1x_rt_dynamic_snapshot_(
      request_path, remaining < ON1X_RT_MAX_DYNAMIC_IMAGE_BYTES
                        ? remaining : ON1X_RT_MAX_DYNAMIC_IMAGE_BYTES,
      &image, &message);
  if (status != ON1X_RT_OK)
    return on1x_rt_engine_fail_(engine, error, status, message);
  ++engine->callback_depth;
  status = on1x_rt_status_normalize_(authorize(
      authorization_context, (On1xRT_StringView){request_path, path.size},
      (On1xRT_StringView){image.sha256, sizeof(image.sha256) - 1u}));
  --engine->callback_depth;
  if (status != ON1X_RT_OK) {
    message = "host did not authorize the measured native image";
    goto failed;
  }
  ++engine->callback_depth;
  status = on1x_rt_dynamic_open_(&image, &library, &entry, &message);
  if (status == ON1X_RT_OK)
    descriptor = entry();
  --engine->callback_depth;
  if (status != ON1X_RT_OK)
    goto failed;

  status = on1x_rt_engine_register_aot_module(engine, descriptor, &published,
                                              error);
  if (status != ON1X_RT_OK) {
    message = "native module registration failed";
    goto failed;
  }
  On1xRT_ModuleObject *object =
      engine->slots[on1x_rt_handle_slot_(published.id)].object;
  object->native_library = library;
  object->native_fd = image.fd;
  object->native_size = image.size;
  engine->native_bytes += image.size;
  *module = published;
  return ON1X_RT_OK;

failed:
  if (library) {
    ++engine->callback_depth;
    on1x_rt_dynamic_close_(library);
    --engine->callback_depth;
  }
  on1x_rt_dynamic_image_release_(image.fd);
  return on1x_rt_engine_fail_(engine, error, status, message);
#endif
}

On1xRT_Status on1x_rt_engine_load_wasm(On1xRT_Engine *engine,
                                       On1xRT_BytesView module_bytes,
                                       On1xRT_Module *module,
                                       On1xRT_Error **error) {
  On1xRT_Status status;
  if (!module || module->id || module->owner ||
      (!module_bytes.data && module_bytes.size) || !on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid engine context")
               : status;
  return on1x_rt_engine_fail_(
      engine, error, ON1X_RT_UNSUPPORTED,
      "WASM execution is unsupported without an engine adapter");
}

On1xRT_Status on1x_rt_engine_enable_concurrency(On1xRT_Engine *engine,
                                                On1xRT_Error **error) {
  On1xRT_Status status;
  if (!on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid engine context")
               : status;
  return on1x_rt_engine_fail_(
      engine, error, ON1X_RT_UNSUPPORTED,
      "runtime concurrency and scheduling are unsupported");
}

On1xRT_Status on1x_rt_engine_attach_debugger(On1xRT_Engine *engine,
                                             On1xRT_Error **error) {
  On1xRT_Status status;
  if (!on1x_rt_error_ready_(error))
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_engine_check_(engine, 0);
  if (status != ON1X_RT_OK)
    return engine && engine->magic == ON1X_RT_ENGINE_MAGIC
               ? on1x_rt_engine_fail_(engine, error, status,
                                      "invalid engine context")
               : status;
  return on1x_rt_engine_fail_(engine, error, ON1X_RT_UNSUPPORTED,
                              "runtime debugger integration is unsupported");
}
