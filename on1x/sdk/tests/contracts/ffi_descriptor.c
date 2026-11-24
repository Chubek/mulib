#include <On1xSDK-FFIDescriptor.h>

#include <stdio.h>
#include <string.h>

#define TEXT(s) ((On1xSDK_FFIText){s, sizeof(s) - 1u})
#define CHECK(e)                                                               \
  do {                                                                         \
    if (!(e)) {                                                                \
      fprintf(stderr, "ffi_descriptor.c:%d: %s\n", __LINE__, #e);               \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int equal(On1xSDK_FFIText a, On1xSDK_FFIText b) {
  return a.size == b.size && (!a.size || !memcmp(a.data, b.data, a.size));
}

int main(int argc, char **argv) {
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

  CHECK(argc == 1 || (argc == 2 && !strcmp(argv[1], "--unavailable")));
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

  if (argc == 2) {
    CHECK(on1x_sdk_ffi_descriptor_find(&query, out) == ON1X_SDK_UNSUPPORTED);
    CHECK(!out->descriptor_schema && !out->descriptor_id.data);
    return 0;
  }
  CHECK(on1x_sdk_ffi_descriptor_find(&query, out) == ON1X_SDK_OK);
  CHECK(ext.future == UINT64_C(0xdeadbeef));
  CHECK(out->struct_size == sizeof(ext));
  CHECK(out->descriptor_schema == query.descriptor_schema);
  CHECK(out->placement == ON1X_SDK_FFI_PLACEMENT_EXTERNAL);
  CHECK(equal(out->target, query.target) &&
        equal(out->module, query.module) && equal(out->member, query.member));
  CHECK(equal(out->native_symbol, TEXT("mulib_clamp_i64")));
  CHECK(equal(out->header, TEXT("mulib-clamp.h")));
  CHECK(equal(out->calling_convention, TEXT("sysv")));
  CHECK(out->parameter_count == 3 && out->guard_count == 1);
  CHECK(out->guards[0].lhs == 1 && out->guards[0].rhs == 2);
  CHECK(out->result.surface_kind == ON1X_SDK_FFI_SURFACE_I64);
  CHECK(out->result.native_kind == ON1X_SDK_FFI_NATIVE_INTEGER);
  CHECK(equal(out->result.native_identity, TEXT("long")));
  CHECK(out->result.native_rank == 4 && out->result.native_bits == 64 &&
        out->result.native_size_bytes == 8 &&
        out->result.native_alignment_bytes == 8 && out->result.native_is_signed);
  CHECK(on1x_sdk_ffi_sha256_valid(out->native_fingerprint));
  CHECK(on1x_sdk_ffi_sha256_valid(out->descriptor_id));
  const On1xSDK_FFIText id = out->descriptor_id;
  bad = query;
  bad.descriptor_id = id;
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_OK);
  bad.module = bad.member = TEXT("");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_OK);
  CHECK(equal(out->descriptor_id, id));

  bad.target = TEXT("aarch64-linux-gnu");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_NOT_FOUND);
  CHECK(!out->descriptor_id.data && !out->parameter_count && !out->guard_count);
  bad = query;
  bad.member = TEXT("I64");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_NOT_FOUND);
  bad = query;
  bad.member = TEXT("");
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_INVALID_ARGUMENT);
  bad = query;
  char stale[64];
  memcpy(stale, id.data, sizeof(stale));
  stale[0] = stale[0] == 'f' ? 'e' : 'f';
  bad.descriptor_id = (On1xSDK_FFIText){stale, sizeof(stale)};
  CHECK(on1x_sdk_ffi_descriptor_find(&bad, out) == ON1X_SDK_STALE);
  CHECK(!out->descriptor_id.data && !out->descriptor_schema &&
        ext.future == UINT64_C(0xdeadbeef));
  return 0;
}
