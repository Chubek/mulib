#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Status.h"
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

struct StoredAnalyzerV2 {
    uint32_t input_schema;
    On1xSDK_AnalyzerFnV2 run;
    void *userdata;
};

struct StagedDiagnosticsV2 {
    std::vector<StagedDiagnostic> entries;
    On1xSDK_Status status = ON1X_SDK_OK;
};

extern "C" On1xSDK_Status stage_diagnostic_v2(void *userdata,
                                               const On1xSDK_DiagnosticV2 *record) noexcept {
    auto &stage = *static_cast<StagedDiagnosticsV2 *>(userdata);
    if (stage.status != ON1X_SDK_OK) return stage.status;
    auto keep = [&](On1xSDK_Status status) { return stage.status = status; };
    if (stage.entries.size() >= ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS)
        return keep(ON1X_SDK_LIMIT);
    // Validate the prefix before reading fields; reject unsupported features
    // before traversing any provider-supplied arrays.
    auto status = on1x_sdk_abi_header_validate(record, sizeof(On1xSDK_DiagnosticV2),
                                               ON1X_SDK_ABI_VERSION_V2, nullptr);
    if (status != ON1X_SDK_OK) return keep(status);
    if (record->related_count || record->fixit_count || record->category.size ||
        record->phase.size || record->provider.size)
        return keep(ON1X_SDK_UNSUPPORTED);
    if (record->message.size > ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES)
        return keep(ON1X_SDK_LIMIT);
    status = on1x_sdk_diagnostic_validate(record, nullptr);
    if (status != ON1X_SDK_OK) return keep(status);
    if (std::memchr(record->message.data, 0, record->message.size))
        return keep(ON1X_SDK_INVALID_ARGUMENT);
    return keep(on1x::sdk::guard([&] {
        stage.entries.push_back({
            record->severity, {record->code.data, record->code.size},
            {record->message.data, record->message.size}, SIZE_MAX, SIZE_MAX
        });
        return ON1X_SDK_OK;
    }));
}

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

On1xSDK_Status run_analyzer_v2(const On1xSDK_Registry *registry, std::string_view name,
                               const On1xSDK_MIRSnapshot *snapshot,
                               const On1xSDK_DiagnosticSinkV2 &diagnostics) noexcept {
    if (!valid_name(name)) return ON1X_SDK_INVALID_ARGUMENT;
    On1xSDK_IRIdentity identity{};
    auto status = on1x_sdk_mir_snapshot_identity(snapshot, &identity);
    if (status != ON1X_SDK_OK) return status;
    if (identity.state != ON1X_SDK_IR_VERIFIED_MIR) return ON1X_SDK_VERIFICATION_FAILED;
    status = check_active_registry_context(registry, identity.owner);
    if (status != ON1X_SDK_OK) return status;
    const auto *descriptor = static_cast<const StoredAnalyzerV2 *>(
        find(registry, Kind::AnalyzerV2, name));
    if (!descriptor || identity.schema_version != descriptor->input_schema)
        return ON1X_SDK_UNSUPPORTED;

    StagedDiagnosticsV2 stage;
    const On1xSDK_DiagnosticSinkV2 sink{
        ON1X_SDK_ABI_VERSION_V2, sizeof sink, stage_diagnostic_v2, &stage
    };
    status = guard([&] {
        const auto result = descriptor->run(descriptor->userdata, snapshot, &sink);
        return on1x_sdk_status_is_known(result) ? result : ON1X_SDK_ERROR;
    });
    if (stage.status != ON1X_SDK_OK) return stage.status;
    if (status != ON1X_SDK_OK) return status;
    for (const auto &entry : stage.entries) {
        On1xSDK_DiagnosticV2 record{};
        record.abi_version = ON1X_SDK_ABI_VERSION_V2;
        record.struct_size = sizeof record;
        record.severity = entry.severity;
        record.code = {entry.code.data(), entry.code.size()};
        record.message = {entry.message.data(), entry.message.size()};
        record.primary = on1x_sdk_span_unlocated();
        status = on1x_sdk_diagnostic_emit_v2(&diagnostics, &record, nullptr);
        if (status != ON1X_SDK_OK) return status;
    }
    return ON1X_SDK_OK;
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

On1xSDK_Status on1x_sdk_analyzer_register_v2(On1xSDK_Registry *registry,
                                            const On1xSDK_AnalyzerV2 *analyzer) {
    using namespace on1x::sdk;
    if (!registry) return ON1X_SDK_INVALID_ARGUMENT;
    constexpr auto required = offsetof(On1xSDK_AnalyzerV2, run) + sizeof(On1xSDK_AnalyzerV2::run);
    const auto status = on1x_sdk_abi_header_validate(
        analyzer, required, ON1X_SDK_ABI_VERSION_V2, nullptr);
    if (status != ON1X_SDK_OK) return status;
    if (!analyzer->name.data || !valid_name({analyzer->name.data, analyzer->name.size}))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (analyzer->reserved || !analyzer->run) return ON1X_SDK_INVALID_ARGUMENT;
    if (analyzer->input_schema != ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION)
        return ON1X_SDK_UNSUPPORTED;
    return guard([&] {
        const StoredAnalyzerV2 descriptor{
            analyzer->input_schema, analyzer->run,
            ON1X_SDK_STRUCT_HAS(analyzer, On1xSDK_AnalyzerV2, userdata) ? analyzer->userdata : nullptr
        };
        const std::string_view name{analyzer->name.data, analyzer->name.size};
        return add(registry, Kind::AnalyzerV2, name,
                   std::make_shared<const StoredAnalyzerV2>(descriptor));
    });
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
