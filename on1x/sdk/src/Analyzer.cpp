#include "On1xSDK-Analyzer.h"
#include "FacetInternal.hpp"
#include "ProgramInternal.hpp"

namespace {

struct StagedDiagnostic {
    On1xSDK_Severity severity;
    std::string code;
    std::string message;
    size_t byte_begin;
    size_t byte_end;
};

On1xSDK_Status make_program(On1xSDK_Phase phase, const void *bytes, size_t size,
                            On1xSDK_Program **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (size > ON1X_SDK_MAX_PROGRAM_BYTES) return ON1X_SDK_LIMIT;
    if (size && !bytes) return ON1X_SDK_INVALID_ARGUMENT;
    return on1x::sdk::guard([&] {
        std::vector<uint8_t> copy(size);
        if (size) std::memcpy(copy.data(), bytes, size);
        *out = new On1xSDK_Program{phase, std::move(copy)};
        return ON1X_SDK_OK;
    });
}

} // namespace

struct On1xSDK_AnalyzerSink {
    std::vector<StagedDiagnostic> entries;
    const size_t program_size;
    On1xSDK_Status status = ON1X_SDK_OK;
};

namespace on1x::sdk {

On1xSDK_Status make_verified_mir(std::string_view canonical_text,
                                 On1xSDK_Program **out) noexcept {
    return make_program(ON1X_SDK_VERIFIED_MIR, canonical_text.data(), canonical_text.size(), out);
}

On1xSDK_Status make_checked_hir(const Unit &unit, std::string_view source_name,
                                On1xSDK_Program **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&] {
        *out = new On1xSDK_Program{ON1X_SDK_CHECKED_HIR, {}, &unit, nullptr, source_name};
        return ON1X_SDK_OK;
    });
}

const Unit *checked_hir(const On1xSDK_Program *program, std::string_view *source_name) noexcept {
    if (source_name) *source_name = program ? program->source_name : std::string_view{};
    return program ? program->checked_unit : nullptr;
}

On1xSDK_Status make_builtin_mir(const mir::Module &module, On1xSDK_Program **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&] {
        *out = new On1xSDK_Program{ON1X_SDK_VERIFIED_MIR, {}, nullptr, &module};
        return ON1X_SDK_OK;
    });
}

const mir::Module *builtin_mir(const On1xSDK_Program *program) noexcept {
    return program ? program->verified_module : nullptr;
}

} // namespace on1x::sdk

extern "C" {

On1xSDK_Status on1x_sdk_program_create_source(const void *bytes, size_t size, On1xSDK_Program **out) {
    return make_program(ON1X_SDK_SOURCE, bytes, size, out);
}

void on1x_sdk_program_destroy(On1xSDK_Program *program) {
    delete program;
}

On1xSDK_Status on1x_sdk_program_phase(const On1xSDK_Program *program, On1xSDK_Phase *phase) {
    if (!program || !phase) return ON1X_SDK_INVALID_ARGUMENT;
    *phase = program->phase;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_program_bytes(const On1xSDK_Program *program,
                                     const uint8_t **bytes, size_t *size) {
    if (program && (program->checked_unit || program->verified_module))
        return ON1X_SDK_UNSUPPORTED;
    return on1x::sdk::view_bytes(program, bytes, size);
}

On1xSDK_Status on1x_sdk_analyzer_emit(On1xSDK_AnalyzerSink *sink,
                                     const On1xSDK_Diagnostic *diagnostic) {
    using namespace on1x::sdk;
    if (!sink) return ON1X_SDK_INVALID_ARGUMENT;
    if (sink->status != ON1X_SDK_OK) return sink->status;
    if (!diagnostic || !diagnostic->message || diagnostic->message[0] == '\0')
        return sink->status = ON1X_SDK_INVALID_ARGUMENT;
    if (sink->entries.size() >= ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS)
        return sink->status = ON1X_SDK_LIMIT;

    const auto &[severity, code, message, begin, end] = *diagnostic;
    std::string_view code_text, message_text;
    if (const auto status = read_name(code, code_text); status != ON1X_SDK_OK)
        return sink->status = status;
    if ((severity != ON1X_SDK_NOTE && severity != ON1X_SDK_WARNING &&
         severity != ON1X_SDK_DIAGNOSTIC_ERROR) ||
        (!(begin == SIZE_MAX && end == SIZE_MAX) && (begin > end || end > sink->program_size)))
        return sink->status = ON1X_SDK_INVALID_ARGUMENT;
    if (!bounded_text(message, ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES, message_text))
        return sink->status = ON1X_SDK_LIMIT;
    return sink->status = guard([&] {
        sink->entries.push_back({severity, std::string(code_text), std::string(message_text), begin, end});
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_analyzer_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Analyzer *analyzer) {
    return on1x::sdk::register_facet(registry, on1x::sdk::Kind::Analyzer, analyzer, true, true);
}

On1xSDK_Status on1x_sdk_analyzer_run(const On1xSDK_Registry *registry, const char *name,
                                    const On1xSDK_Program *program,
                                    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk;
    std::string_view key;
    if (!registry || !program) return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status = read_name(name, key); status != ON1X_SDK_OK) return status;
    const auto *descriptor = find_facet<On1xSDK_Analyzer>(registry, Kind::Analyzer, key);
    if (!descriptor || program->phase != descriptor->phase) return ON1X_SDK_UNSUPPORTED;

    const auto host_sink = diagnostics ? *diagnostics : On1xSDK_DiagnosticSink{};
    On1xSDK_AnalyzerSink sink{{}, program->bytes.size()};
    const auto status = invoke(program, descriptor->run, descriptor->userdata, sink);
    if (status != ON1X_SDK_OK || !host_sink.emit) return status;

    return guard([&] {
        for (const auto &entry : sink.entries) {
            const On1xSDK_Diagnostic record{entry.severity, entry.code.c_str(), entry.message.c_str(),
                                            entry.byte_begin, entry.byte_end};
            host_sink.emit(host_sink.userdata, &record);
        }
        return ON1X_SDK_OK;
    });
}

} // extern "C"
