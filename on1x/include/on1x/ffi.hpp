#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace on1x::ffi {
enum class SurfaceKind { Void, I64, U8, U16, U32, U64, F32, F64, Bool };
enum class NativeKind { Void, Integer, Real };
enum class Placement { External, StaticInline };
enum class Effect { Unknown };

struct Target {
    std::string_view triple, data_model, abi, calling_convention;
    friend constexpr bool operator==(Target, Target) = default;
};

struct NativeType {
    NativeKind kind;
    std::string_view identity, format;
    std::uint16_t rank, bits, size, alignment;
    bool is_signed;
    friend constexpr bool operator==(NativeType, NativeType) = default;
};

struct Parameter {
    std::string_view name;
    SurfaceKind surface;
    NativeType native;
};

struct Guard { std::uint8_t lhs, rhs; };

// Evaluate all surface arguments once in order, check the guards on those
// values, convert (with range checks as needed), call once, convert the result.
struct ScalarPlan { std::span<const Guard> guards; };

// An identifier names a candidate, not evidence of provenance or admission.
// Resolve against the compiler-owned table and independently check admission.
struct ForeignId {
    std::string_view sha256;
    friend constexpr bool operator==(ForeignId, ForeignId) = default;
};

struct Descriptor {
    std::string_view module, provider, header, member, symbol;
    Target target;
    Placement placement;
    std::string_view definition;
    std::span<const Parameter> parameters;
    SurfaceKind surface_result;
    NativeType native_result;
    ScalarPlan plan;
    Effect effect;
    std::string_view native_fingerprint;
    ForeignId id;
    std::uint8_t schema_version; // 1: legacy i64 seed, 2: explicit native projection
};

// All returned views point to immutable compiler-owned storage. A descriptor
// only proves representation eligibility; these lookups never authorize calls.
std::span<const Descriptor> foreign_descriptors() noexcept;
// Selected at build time, even if the immutable registry has no exports.
std::string_view configured_target() noexcept;
const Descriptor* find_descriptor(std::string_view module, std::string_view member) noexcept;
const Descriptor* find_descriptor(ForeignId id) noexcept;
const Descriptor* find_descriptor_by_symbol(std::string_view symbol) noexcept;
}
