#include <On1xSDK-FFIDescriptor.h>

#include <stdio.h>

#define TEXT(s) ((On1xSDK_FFIText){s, sizeof(s) - 1u})
#define CHECK(e)                                                               \
  do {                                                                         \
    if (!(e)) {                                                                \
      fprintf(stderr, "ffi_descriptor.c:%d: %s\n", __LINE__, #e);               \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  const uint32_t abi = ON1X_SDK_ABI_VERSION_V2;
  const On1xSDK_FFIDescriptorQuery query = {
      abi, sizeof(query), ON1X_SDK_FFI_DESCRIPTOR_SCHEMA_V2,
      TEXT("x86_64-linux-gnu"), TEXT("mulib.clamp"), TEXT("i64"), {0}};
  On1xSDK_FFIDescriptorQuery bad = query;
  struct {
    On1xSDK_FFIDescriptorView view;
    uint64_t future;
  } ext = {.view = {.abi_version = abi, .struct_size = sizeof(ext)},
           .future = UINT64_C(0xdeadbeef)};
  On1xSDK_FFIDescriptorView *out = &ext.view;

  CHECK(on1x_sdk_ffi_descriptor_find(&query, NULL) == ON1X_SDK_INVALID_ARGUMENT);
  CHECK(on1x_sdk_ffi_descriptor_find(NULL, out) == ON1X_SDK_INVALID_ARGUMENT);
  CHECK(out->abi_version == abi && out->struct_size == sizeof(ext));
  CHECK(ext.future == UINT64_C(0xdeadbeef));
  out->abi_version = ON1X_SDK_ABI_VERSION_V1;
  CHECK(on1x_sdk_ffi_descriptor_find(&query, out) == ON1X_SDK_ABI_MISMATCH);
  out->abi_version = abi;
  out->struct_size = 8;
  CHECK(on1x_sdk_ffi_descriptor_find(&query, out) == ON1X_SDK_INVALID_ARGUMENT);
  out->struct_size = sizeof(ext);
  bad.abi_version = ON1X_SDK_ABI_VERSION_V1;
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_ABI_MISMATCH);
  bad = query;
  bad.struct_size = 8;
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.descriptor_schema = 1;
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_UNSUPPORTED);
  bad = query;
  bad.target = TEXT("");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.member = TEXT("i64\0more");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.target = TEXT("\xc0\x80");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.module = bad.member = TEXT("");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.module.size = SIZE_MAX;
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  bad.descriptor_id = TEXT("forged review");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);

  CHECK(on1x_sdk_ffi_descriptor_find(&query, out) == ON1X_SDK_UNSUPPORTED);
  CHECK(!out->descriptor_schema && !out->descriptor_id.data);
  CHECK(ext.future == UINT64_C(0xdeadbeef));
  return 0;
}
