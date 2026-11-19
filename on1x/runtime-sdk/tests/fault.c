#include "fixture_module.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FaultAllocator FaultAllocator;

typedef struct AllocationHeader {
  void *base;
  FaultAllocator *owner;
  size_t size;
  size_t alignment;
  uint64_t magic;
} AllocationHeader;

struct FaultAllocator {
  size_t calls;
  size_t fail_at;
  size_t live;
  size_t peak;
  int corrupt;
};

#define ALLOCATION_MAGIC UINT64_C(0x616c6c6f63363178)

static void *fault_allocate(void *opaque, size_t size, size_t alignment) {
  FaultAllocator *allocator = (FaultAllocator *)opaque;
  AllocationHeader *header;
  uintptr_t address;
  void *base;
  size_t amount;
  ++allocator->calls;
  if (allocator->fail_at && allocator->calls == allocator->fail_at)
    return NULL;
  if (!size || !alignment || (alignment & (alignment - 1u)) ||
      size > SIZE_MAX - alignment - sizeof(*header))
    return NULL;
  amount = size + alignment - 1u + sizeof(*header);
  base = malloc(amount);
  if (!base)
    return NULL;
  address = (uintptr_t)base + sizeof(*header);
  address = (address + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
  header = ((AllocationHeader *)address) - 1;
  header->base = base;
  header->owner = allocator;
  header->size = size;
  header->alignment = alignment;
  header->magic = ALLOCATION_MAGIC;
  ++allocator->live;
  if (allocator->live > allocator->peak)
    allocator->peak = allocator->live;
  return (void *)address;
}

static void fault_release(void *opaque, void *pointer, size_t size,
                          size_t alignment) {
  FaultAllocator *allocator = (FaultAllocator *)opaque;
  AllocationHeader *header;
  if (!pointer)
    return;
  header = ((AllocationHeader *)pointer) - 1;
  if (header->magic != ALLOCATION_MAGIC || header->owner != allocator ||
      header->size != size || header->alignment != alignment ||
      !allocator->live) {
    allocator->corrupt = 1;
    return;
  }
  header->magic = 0u;
  --allocator->live;
  free(header->base);
}

static void *fault_reallocate(void *opaque, void *pointer, size_t old_size,
                              size_t new_size, size_t alignment) {
  void *replacement;
  if (!pointer)
    return fault_allocate(opaque, new_size, alignment);
  if (!new_size) {
    fault_release(opaque, pointer, old_size, alignment);
    return NULL;
  }
  replacement = fault_allocate(opaque, new_size, alignment);
  if (!replacement)
    return NULL;
  memcpy(replacement, pointer, old_size < new_size ? old_size : new_size);
  fault_release(opaque, pointer, old_size, alignment);
  return replacement;
}

static void clear_error(On1xRT_Error **error) {
  if (*error && on1x_rt_error_release(error) != ON1X_RT_OK)
    abort();
}

static int exercise(size_t fail_at, size_t *calls, size_t *peak) {
  static const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_I64,
                                                ON1X_RT_VALUE_I64};
  static const On1xRT_Signature signature = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature),
      parameters,          2u,
      ON1X_RT_VALUE_I64,   0u};
  FaultAllocator tracker = {0u, fail_at, 0u, 0u, 0};
  On1xRT_Allocator allocator = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Allocator), &tracker,
      fault_allocate,      fault_reallocate,         fault_release};
  On1xRT_EngineConfig config;
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_Function function = {0};
  On1xRT_Value result = on1x_rt_value_void();
  On1xRT_Status status;
  int completed = 0;
  memset(&config, 0, sizeof(config));
  config.abi_version = ON1X_RT_ABI_VERSION;
  config.struct_size = sizeof(config);
  config.allocator = &allocator;
  config.max_handles = 16u;
  config.max_modules = 4u;
  config.max_host_functions = 4u;
  config.max_value_bytes = 4096u;
  on1x_rt_test_fixture_reset();
  status = on1x_rt_engine_create(&config, &engine, &error);
  if (status != ON1X_RT_OK) {
    clear_error(&error);
    goto done;
  }
  on1x_rt_test_fixture_set_engine(engine);
  status = on1x_rt_engine_register_static_module(
      engine, ON1X_RT_MODULE_ENTRY_SYMBOL(), &module, &error);
  if (status != ON1X_RT_OK) {
    clear_error(&error);
    goto cleanup_engine;
  }
  status = on1x_rt_instance_create(engine, module, &instance, &error);
  if (status != ON1X_RT_OK) {
    clear_error(&error);
    goto cleanup_module;
  }
  status = on1x_rt_instance_function(engine, instance,
                                     (On1xRT_StringView){"add", 3u}, &signature,
                                     &function, &error);
  if (status != ON1X_RT_OK) {
    clear_error(&error);
    goto cleanup_instance;
  }
  {
    const On1xRT_Value arguments[] = {on1x_rt_value_i64(10),
                                      on1x_rt_value_i64(20)};
    status =
        on1x_rt_function_call(engine, function, arguments, 2u, &result, &error);
    if (status == ON1X_RT_OK) {
      if (result.kind != ON1X_RT_VALUE_I64 || result.as.i64 != 31)
        tracker.corrupt = 1;
      if (on1x_rt_value_release(engine, &result, &error) != ON1X_RT_OK)
        tracker.corrupt = 1;
    } else {
      clear_error(&error);
    }
  }
  if (on1x_rt_function_release(engine, &function, &error) != ON1X_RT_OK)
    tracker.corrupt = 1;
  clear_error(&error);
  completed = status == ON1X_RT_OK;

cleanup_instance:
  if (instance.id &&
      on1x_rt_instance_release(engine, &instance, &error) != ON1X_RT_OK)
    tracker.corrupt = 1;
  clear_error(&error);
cleanup_module:
  if (module.id && on1x_rt_module_unload(engine, &module, &error) != ON1X_RT_OK)
    tracker.corrupt = 1;
  clear_error(&error);
cleanup_engine:
  if (on1x_rt_engine_destroy(&engine, &error) != ON1X_RT_OK)
    tracker.corrupt = 1;
  clear_error(&error);
done:
  *calls = tracker.calls;
  *peak = tracker.peak;
  if (tracker.live || tracker.corrupt) {
    fprintf(stderr,
            "fault point %zu leaked/corrupted allocations "
            "(live=%zu calls=%zu peak=%zu)\n",
            fail_at, tracker.live, tracker.calls, tracker.peak);
    return -1;
  }
  return completed;
}

typedef struct MisalignedAllocator {
  size_t allocations;
  size_t releases;
} MisalignedAllocator;

static void *misaligned_allocate(void *opaque, size_t size, size_t alignment) {
  MisalignedAllocator *allocator = (MisalignedAllocator *)opaque;
  unsigned char *base;
  (void)alignment;
  base = (unsigned char *)malloc(size + 1u);
  if (!base)
    return NULL;
  ++allocator->allocations;
  return base + 1u;
}

static void misaligned_release(void *opaque, void *pointer, size_t size,
                               size_t alignment) {
  MisalignedAllocator *allocator = (MisalignedAllocator *)opaque;
  (void)size;
  (void)alignment;
  if (pointer) {
    ++allocator->releases;
    free((unsigned char *)pointer - 1u);
  }
}

static void *misaligned_reallocate(void *opaque, void *pointer, size_t old_size,
                                   size_t new_size, size_t alignment) {
  (void)old_size;
  if (pointer)
    misaligned_release(opaque, pointer, old_size, alignment);
  return new_size ? misaligned_allocate(opaque, new_size, alignment) : NULL;
}

int main(void) {
  size_t successful = 0u;
  size_t failures = 0u;
  size_t maximum_calls = 0u;
  size_t maximum_peak = 0u;
  for (size_t point = 1u; point <= 160u; ++point) {
    size_t calls = 0u;
    size_t peak = 0u;
    const int result = exercise(point, &calls, &peak);
    if (result < 0)
      return 1;
    if (result)
      ++successful;
    else
      ++failures;
    if (calls > maximum_calls)
      maximum_calls = calls;
    if (peak > maximum_peak)
      maximum_peak = peak;
  }
  if (!failures || successful < 8u || maximum_calls < 10u ||
      maximum_peak < 5u) {
    fprintf(stderr,
            "fault sweep lacked coverage: failures=%zu success=%zu "
            "calls=%zu peak=%zu\n",
            failures, successful, maximum_calls, maximum_peak);
    return 2;
  }
  {
    MisalignedAllocator tracker = {0u, 0u};
    On1xRT_Allocator allocator = {
        ON1X_RT_ABI_VERSION, sizeof(On1xRT_Allocator), &tracker,
        misaligned_allocate, misaligned_reallocate,    misaligned_release};
    On1xRT_EngineConfig config;
    On1xRT_Engine *engine = NULL;
    On1xRT_Error *error = NULL;
    memset(&config, 0, sizeof(config));
    config.abi_version = ON1X_RT_ABI_VERSION;
    config.struct_size = sizeof(config);
    config.allocator = &allocator;
    if (on1x_rt_engine_create(&config, &engine, &error) !=
            ON1X_RT_OUT_OF_MEMORY ||
        engine != NULL || error != NULL || !tracker.allocations ||
        tracker.allocations != tracker.releases) {
      fprintf(stderr, "misaligned allocator was not rejected safely\n");
      return 3;
    }
  }
  return 0;
}
