#include "Bridge.hpp"

#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"
#include "backend/BackendProviderInternal.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <string_view>

#ifndef ON1X_ARCHON_TARGET_TRIPLE
#error "Define ON1X_ARCHON_TARGET_TRIPLE to the generated Archon backend target's exact On1x SDK triple"
#endif

namespace {
bool matches_generated_architecture(std::string_view triple) noexcept {
    const char *raw = archon_backend_target();
    if (!raw) return false;
    const std::string_view native(raw);
    // IRGen pins a target/ABI in the generated library. An SDK triple is an
    // *additional* assertion; it must not silently select a different ISA.
    if (triple.starts_with("x86_64-")) return native.find("AMD64") != native.npos;
    if (triple.starts_with("aarch64-")) return native.find("AArch64") != native.npos;
    if (triple.starts_with("riscv64-")) return native.find("RISCV64") != native.npos;
    return false;
}
int cancelled_by_host(void *state) noexcept {
    const auto *call = static_cast<const On1xSDK_BackendInvocation *>(state);
    return on1x_sdk_backend_poll(call) == ON1X_SDK_OK ? 0 : 1;
}

On1xSDK_Status emit_archon(void *userdata, const On1xSDK_BackendInvocation *invocation,
                           On1xSDK_OutputSink *sink) noexcept {
    if (!userdata || !invocation || !sink || !invocation->program || !invocation->target ||
        invocation->artifact_kind != ON1X_SDK_ARTIFACT_ASSEMBLY)
        return ON1X_SDK_INVALID_ARGUMENT;
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    context.diagnostic.clear();
    try {
        const On1xSDK_TargetDescriptor *target = nullptr;
        auto status = on1x_sdk_target_view(invocation->target, &target);
        if (status != ON1X_SDK_OK) return status;
        constexpr std::string_view expected = ON1X_ARCHON_TARGET_TRIPLE;
        if (!target || !target->triple || target->triple_size != expected.size() ||
            std::memcmp(target->triple, expected.data(), expected.size()) ||
            !matches_generated_architecture(expected))
            return ON1X_SDK_UNSUPPORTED;

        on1x::backends::archon::Options opt;
        opt.emit_c_entrypoint = true;
        opt.max_assembly_bytes = std::min<std::size_t>(invocation->byte_limit,
                                                       ON1X_SDK_MAX_ARTIFACT_BYTES);
        opt.cancelled = cancelled_by_host;
        opt.cancel_userdata = const_cast<On1xSDK_BackendInvocation *>(invocation);
        if (!opt.max_assembly_bytes) return ON1X_SDK_LIMIT;
        const auto write = [&](const on1x::mir::Module &module) {
            auto product = on1x::backends::archon::compile(module, opt);
            return on1x_sdk_output_write(sink, product.assembly.data(), product.assembly.size());
        };
        if (const auto *module = on1x::sdk::builtin_mir(invocation->program))
            return write(*module);
        const std::uint8_t *bytes = nullptr;
        std::size_t size = 0;
        status = on1x_sdk_program_bytes(invocation->program, &bytes, &size);
        if (status != ON1X_SDK_OK) return status;
        if (!bytes && size) return ON1X_SDK_INVALID_ARGUMENT;
        const auto wire = size ? std::string_view(reinterpret_cast<const char *>(bytes), size)
                               : std::string_view{};
        return write(on1x::mir::deserialize(wire));
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}
} // namespace

extern "C" On1xSDK_Status on1x_builtin_mir_archon_register(
    On1xSDK_Registry *registry, on1x::backends::Context *context) {
    if (!registry || !context) return ON1X_SDK_INVALID_ARGUMENT;
    if (!matches_generated_architecture(ON1X_ARCHON_TARGET_TRIPLE))
        return ON1X_SDK_UNSUPPORTED;
    static constexpr char triple[] = ON1X_ARCHON_TARGET_TRIPLE;
    static const On1xSDK_BackendTarget target{triple, sizeof(triple) - 1};
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 1, 3, ON1X_SDK_ARTIFACT_ASSEMBLY,
        ON1X_SDK_BACKEND_DETERMINISTIC, &target, 1, nullptr, 0, nullptr, 0
    };
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-archon", sizeof("mir-archon") - 1, 0, 0, &capability, 1,
        emit_archon, context
    };
    // The host must enable its verified native-artifact policy first. This
    // registration intentionally does NOT mislabel assembly as C11 source.
    return on1x::sdk::register_builtin_backend_provider(registry, &provider);
}
