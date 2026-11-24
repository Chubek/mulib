#include <On1xSDK-FFIDescriptor.h>

#include <cstdio>
#include <string_view>

#ifdef ON1X_SDK_FFI_TEST_CANONICAL
#include <on1x/ffi.hpp>
#endif

namespace {
constexpr On1xSDK_FFIText text(std::string_view value) {
    return {value.data(), value.size()};
}

bool equal(On1xSDK_FFIText a, std::string_view b) {
    return a.size == b.size() && (!a.size || std::string_view(a.data, a.size) == b);
}
} // namespace

#define CHECK(e) do { \
    if (!(e)) { \
        std::fprintf(stderr, "ffi_descriptor.cpp:%d: %s\n", __LINE__, #e); \
        return 1; \
    } \
} while (0)

int main() {
    On1xSDK_FFIDescriptorQuery query{};
    query.abi_version = ON1X_SDK_ABI_VERSION_V2;
    query.struct_size = sizeof(query);
    query.descriptor_schema = ON1X_SDK_FFI_DESCRIPTOR_SCHEMA_V2;
    query.target = text("x86_64-linux-gnu");
    query.module = text("mulib.clamp");
    query.member = text("add_i64");
    On1xSDK_FFIDescriptorView out{};
    out.abi_version = ON1X_SDK_ABI_VERSION_V2;
    out.struct_size = sizeof(out);
    CHECK(on1x_sdk_ffi_descriptor_find(&query, &out) == ON1X_SDK_OK);
    CHECK(equal(out.native_symbol, "mulib_clamp_add_i64"));
    CHECK(out.parameter_count == 2 && out.guard_count == 0);

#ifdef ON1X_SDK_FFI_TEST_CANONICAL
    namespace ffi = on1x::ffi;
    const auto *d = ffi::find_descriptor("mulib.clamp", "add_i64");
    CHECK(d != nullptr);
    CHECK(equal(out.descriptor_id, d->id.sha256));
    CHECK(equal(out.native_fingerprint, d->native_fingerprint));
    /* Borrow the compiler's own immutable bytes, not a reconstructed table. */
    CHECK(out.descriptor_id.data == d->id.sha256.data());
    CHECK(out.native_symbol.data == d->symbol.data());
    CHECK(out.parameters[0].type.native_identity.data ==
          d->parameters[0].native.identity.data());

    if (const auto *inl = ffi::find_descriptor("mulib.container_core", "counter_add");
        inl && inl->schema_version == ON1X_SDK_FFI_DESCRIPTOR_SCHEMA_V2) {
        query.module = text(inl->module);
        query.member = text(inl->member);
        CHECK(on1x_sdk_ffi_descriptor_find(&query, &out) == ON1X_SDK_OK);
        CHECK(out.placement == ON1X_SDK_FFI_PLACEMENT_STATIC_INLINE);
        CHECK(out.result.surface_kind == ON1X_SDK_FFI_SURFACE_U64);
        CHECK(equal(out.result.native_identity, "unsigned-long"));
        CHECK(out.descriptor_id.data == inl->id.sha256.data());
    }
#endif
    return 0;
}
