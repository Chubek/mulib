#include "On1xSDK-FFIDescriptor.h"

#if defined(ON1X_SDK_HAS_TYPED_FFI)
#include <on1x/ffi.hpp>

#include <string_view>
#endif

namespace {

#if defined(ON1X_SDK_HAS_TYPED_FFI)
namespace ffi = on1x::ffi;

On1xSDK_FFIText view(std::string_view text) noexcept {
    return {text.data(), text.size()};
}

std::string_view text(On1xSDK_FFIText value) noexcept {
    return value.size ? std::string_view{value.data, value.size}
                      : std::string_view{};
}

uint32_t surface_kind(ffi::SurfaceKind kind) noexcept {
    switch (kind) {
    case ffi::SurfaceKind::Void: return ON1X_SDK_FFI_SURFACE_VOID;
    case ffi::SurfaceKind::I64:  return ON1X_SDK_FFI_SURFACE_I64;
    case ffi::SurfaceKind::U8:   return ON1X_SDK_FFI_SURFACE_U8;
    case ffi::SurfaceKind::U16:  return ON1X_SDK_FFI_SURFACE_U16;
    case ffi::SurfaceKind::U32:  return ON1X_SDK_FFI_SURFACE_U32;
    case ffi::SurfaceKind::U64:  return ON1X_SDK_FFI_SURFACE_U64;
    case ffi::SurfaceKind::F32:  return ON1X_SDK_FFI_SURFACE_F32;
    case ffi::SurfaceKind::F64:  return ON1X_SDK_FFI_SURFACE_F64;
    case ffi::SurfaceKind::Bool: return ON1X_SDK_FFI_SURFACE_BOOL;
    }
    return 0;
}

uint32_t native_kind(ffi::NativeKind kind) noexcept {
    switch (kind) {
    case ffi::NativeKind::Void:    return ON1X_SDK_FFI_NATIVE_VOID;
    case ffi::NativeKind::Integer: return ON1X_SDK_FFI_NATIVE_INTEGER;
    case ffi::NativeKind::Real:    return ON1X_SDK_FFI_NATIVE_REAL;
    }
    return 0;
}

bool scalar_type(ffi::SurfaceKind surface, const ffi::NativeType &native,
                 On1xSDK_FFIScalarTypeV2 &out) noexcept {
    out = {surface_kind(surface), native_kind(native.kind),
           view(native.identity), view(native.format), native.rank,
           native.bits, native.size, native.alignment, native.is_signed};
    return out.surface_kind && out.native_kind;
}

bool project(const ffi::Descriptor &d, On1xSDK_FFIDescriptorView &out) noexcept {
    if (d.parameters.size() > ON1X_SDK_FFI_DESCRIPTOR_MAX_PARAMETERS ||
        d.plan.guards.size() > ON1X_SDK_FFI_DESCRIPTOR_MAX_GUARDS ||
        d.effect != ffi::Effect::Unknown)
        return false;
    switch (d.placement) {
    case ffi::Placement::External:
        out.placement = ON1X_SDK_FFI_PLACEMENT_EXTERNAL;
        break;
    case ffi::Placement::StaticInline:
        out.placement = ON1X_SDK_FFI_PLACEMENT_STATIC_INLINE;
        break;
    default: return false;
    }
    out.descriptor_schema = d.schema_version;
    out.module = view(d.module);
    out.member = view(d.member);
    out.provider = view(d.provider);
    out.header = view(d.header);
    out.native_symbol = view(d.symbol);
    out.native_definition = view(d.definition);
    out.target = view(d.target.triple);
    out.data_model = view(d.target.data_model);
    out.native_abi = view(d.target.abi);
    out.calling_convention = view(d.target.calling_convention);
    out.descriptor_id = view(d.id.sha256);
    out.native_fingerprint = view(d.native_fingerprint);
    if (!scalar_type(d.surface_result, d.native_result, out.result))
        return false;
    for (size_t i = 0; i < d.parameters.size(); ++i) {
        const auto &p = d.parameters[i];
        out.parameters[i].name = view(p.name);
        if (!scalar_type(p.surface, p.native, out.parameters[i].type))
            return false;
    }
    for (size_t i = 0; i < d.plan.guards.size(); ++i)
        out.guards[i] = {d.plan.guards[i].lhs, d.plan.guards[i].rhs};
    out.parameter_count = static_cast<uint32_t>(d.parameters.size());
    out.guard_count = static_cast<uint32_t>(d.plan.guards.size());
    return true;
}
#endif
} // namespace

extern "C" On1xSDK_Status
on1x_sdk_ffi_descriptor_find(const On1xSDK_FFIDescriptorQuery *query,
                            On1xSDK_FFIDescriptorView *out) {
    constexpr auto abi = ON1X_SDK_ABI_VERSION_V2;
    if (!out)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (out->abi_version != abi)
        return ON1X_SDK_ABI_MISMATCH;
    if (out->struct_size < sizeof(*out))
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto capacity = out->struct_size;
    *out = {};
    out->abi_version = abi;
    out->struct_size = capacity;

    if (!query)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (query->abi_version != abi)
        return ON1X_SDK_ABI_MISMATCH;
    if (query->struct_size < sizeof(*query))
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto by_name = query->module.size != 0 || query->member.size != 0;
    const auto pinned = query->descriptor_id.size != 0;
    if (!on1x_sdk_ffi_text_valid(query->target, 0) ||
        (by_name && (!on1x_sdk_ffi_text_valid(query->module, 0) ||
                     !on1x_sdk_ffi_text_valid(query->member, 0))) ||
        (!by_name && !pinned) ||
        (pinned && !on1x_sdk_ffi_sha256_valid(query->descriptor_id)))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (query->descriptor_schema != ON1X_SDK_FFI_DESCRIPTOR_SCHEMA_V2)
        return ON1X_SDK_UNSUPPORTED;
#if defined(ON1X_SDK_HAS_TYPED_FFI)
    const auto id = text(query->descriptor_id);
    const auto *d = by_name
        ? ffi::find_descriptor(text(query->module), text(query->member))
        : ffi::find_descriptor(ffi::ForeignId{id});
    if (!d || d->target.triple != text(query->target))
        return ON1X_SDK_NOT_FOUND;
    if (d->schema_version != query->descriptor_schema)
        return ON1X_SDK_UNSUPPORTED;
    if (pinned && d->id.sha256 != id)
        return ON1X_SDK_STALE;

    On1xSDK_FFIDescriptorView candidate{};
    candidate.abi_version = abi;
    candidate.struct_size = capacity;
    if (!project(*d, candidate))
        return ON1X_SDK_UNSUPPORTED;
    *out = candidate;
    return ON1X_SDK_OK;
#else
    return ON1X_SDK_UNSUPPORTED;
#endif
}
