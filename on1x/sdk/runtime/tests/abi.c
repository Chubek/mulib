#include "On1xRT.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(On1xRT_Status) == 4u, "status ABI drift");
_Static_assert(sizeof(On1xRT_ValueKind) == 4u, "value kind ABI drift");
_Static_assert(sizeof(On1xRT_Module) == 16u, "module handle ABI drift");
_Static_assert(sizeof(On1xRT_Instance) == 16u, "instance handle ABI drift");
_Static_assert(sizeof(On1xRT_Function) == 16u, "function handle ABI drift");
_Static_assert(offsetof(On1xRT_Module, id) == 0u, "handle ID ABI drift");
_Static_assert(offsetof(On1xRT_Module, owner) == 8u, "handle owner ABI drift");

_Static_assert(offsetof(On1xRT_Allocator, abi_version) == 0u,
               "allocator prefix ABI drift");
_Static_assert(offsetof(On1xRT_Allocator, struct_size) == 4u,
               "allocator prefix ABI drift");
_Static_assert(offsetof(On1xRT_Value, kind) == 8u, "value kind ABI drift");
_Static_assert(offsetof(On1xRT_Value, as) == 16u, "value payload ABI drift");

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(On1xRT_Allocator) == 40u, "LP64 allocator ABI drift");
_Static_assert(sizeof(On1xRT_EngineConfig) == 64u,
               "LP64 engine configuration ABI drift");
_Static_assert(sizeof(On1xRT_Value) == 32u, "LP64 value ABI drift");
_Static_assert(sizeof(On1xRT_Signature) == 32u, "LP64 signature ABI drift");
_Static_assert(sizeof(On1xRT_ExportDescriptor) == 80u,
               "LP64 export descriptor ABI drift");
_Static_assert(sizeof(On1xRT_ModuleDescriptor) == 136u,
               "LP64 module descriptor ABI drift");
#endif

_Static_assert(ON1X_RT_OK == 0, "status number changed");
_Static_assert(ON1X_RT_UNSUPPORTED == 2, "status number changed");
_Static_assert(ON1X_RT_INVALID_ARGUMENT == 4, "status number changed");
_Static_assert(ON1X_RT_STALE_HANDLE == 8, "status number changed");
_Static_assert(ON1X_RT_REENTRANT == 15, "status number changed");
_Static_assert(ON1X_RT_DENIED == 17, "status number changed");

int main(void) {
  return ON1X_RT_ABI_VERSION == 1u && ON1X_RT_MODULE_DESCRIPTOR_VERSION == 1u
             ? 0
             : 1;
}
