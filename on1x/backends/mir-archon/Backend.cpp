#include "Bridge.hpp"

#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"
#include "backend/BackendProviderInternal.hpp"

#include <on1x/backend/mir_directives.hpp>

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
    // This is an ABI allowlist, not an ISA substring test. In particular, a
    // Windows triple or another generated calling convention is not SysV.
    if (triple == "x86_64-unknown-linux-gnu") return native == "AMD64/SysVAMD64";
    if (triple == "aarch64-unknown-linux-gnu") return native == "AArch64/AAPCS64";
    if (triple == "riscv64-unknown-linux-gnu") return native == "RISCV64IM/LP64";
    return false;
}
bool matches_layout(const On1xSDK_TargetDescriptor &target) noexcept {
    const auto &layout = target.data_layout;
    if (layout.byte_order != ON1X_SDK_BYTE_ORDER_LITTLE ||
        layout.pointer_bits != 64 || layout.pointer_alignment != 64 ||
        layout.integer_alignment != 64 || layout.stack_alignment != 128 ||
        target.object_format != ON1X_SDK_OBJECT_FORMAT_ELF)
        return false;
    constexpr std::string_view triple = ON1X_ARCHON_TARGET_TRIPLE;
    const auto calling = triple.starts_with("x86_64-")
        ? ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64
        : triple.starts_with("aarch64-") ? ON1X_SDK_CALLING_CONVENTION_AAPCS64
                                       : ON1X_SDK_CALLING_CONVENTION_RISCV_LP64;
    return target.calling_convention == calling;
}
struct Cancellation {
    const On1xSDK_BackendInvocation *invocation;
    On1xSDK_Status status = ON1X_SDK_OK;
};
int cancelled_by_host(void *state) noexcept {
    auto &cancel = *static_cast<Cancellation *>(state);
    if (cancel.status == ON1X_SDK_OK)
        cancel.status = on1x_sdk_backend_poll(cancel.invocation);
    return cancel.status == ON1X_SDK_OK ? 0 : 1;
}

On1xSDK_Status emit_archon(void *userdata, const On1xSDK_BackendInvocation *invocation,
                           On1xSDK_OutputSink *sink) noexcept {
    if (!userdata || !invocation || !sink || !invocation->program || !invocation->target ||
        invocation->artifact_kind != ON1X_SDK_ARTIFACT_ASSEMBLY)
        return ON1X_SDK_INVALID_ARGUMENT;
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    context.diagnostic.clear();
    Cancellation cancel{invocation};
    try {
        const On1xSDK_TargetDescriptor *target = nullptr;
        auto status = on1x_sdk_target_view(invocation->target, &target);
        if (status != ON1X_SDK_OK) return status;
        constexpr std::string_view expected = ON1X_ARCHON_TARGET_TRIPLE;
        if (!target || !target->triple || target->triple_size != expected.size() ||
            std::memcmp(target->triple, expected.data(), expected.size()) ||
            !matches_generated_architecture(expected) || !matches_layout(*target))
            return ON1X_SDK_UNSUPPORTED;

        on1x::backends::archon::Options opt;
        opt.emit_c_entrypoint = context.native_emit_c_entrypoint;
        opt.pic = context.native_pic;
        opt.max_assembly_bytes = std::min<std::size_t>(invocation->byte_limit,
                                                       ON1X_SDK_MAX_ARTIFACT_BYTES);
        opt.cancelled = cancelled_by_host;
        opt.cancel_userdata = &cancel;
        if (!opt.max_assembly_bytes) return ON1X_SDK_LIMIT;
        const auto write = [&](const on1x::mir::Module &module) {
            if (module.version != invocation->input_schema)
                return ON1X_SDK_INVALID_ARGUMENT;
            if (cancelled_by_host(&cancel)) return cancel.status;
            const auto product = module.version != on1x::mir::directive_format_version
                ? on1x::backends::archon::compile(module, opt)
                : on1x::backends::archon::compile(
                    on1x::mir::specialize(module, "mir-archon", expected, context.mir_speculate), opt);
            if (cancelled_by_host(&cancel)) return cancel.status;
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
    } catch (const on1x::backends::archon::Error &error) {
        context.error = std::current_exception();
        if (cancel.status != ON1X_SDK_OK) return cancel.status;
        switch (error.status) {
        case ARCHON_BACKEND_LIMIT:
        case ARCHON_BACKEND_NOMEM: return ON1X_SDK_LIMIT;
        case ARCHON_BACKEND_CANCELLED: return ON1X_SDK_CANCELLED;
        case ARCHON_BACKEND_UNSUPPORTED: return ON1X_SDK_UNSUPPORTED;
        case ARCHON_BACKEND_INVALID: return ON1X_SDK_VERIFICATION_FAILED;
        default: return ON1X_SDK_ERROR;
        }
    } catch (const on1x::Error &) {
        context.error = std::current_exception();
        return cancel.status != ON1X_SDK_OK ? cancel.status : ON1X_SDK_VERIFICATION_FAILED;
    } catch (...) {
        context.error = std::current_exception();
        return cancel.status != ON1X_SDK_OK ? cancel.status : ON1X_SDK_ERROR;
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
        ON1X_SDK_VERIFIED_MIR, 1, on1x::mir::directive_format_version, ON1X_SDK_ARTIFACT_ASSEMBLY,
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
