#include "CoreInternal.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <string>

namespace {

bool contains_nul(On1xSDK_StringView text) noexcept {
    return text.size && std::memchr(text.data, 0, text.size);
}

On1xSDK_Status optional_identifier(On1xSDK_StringView text) noexcept {
    if (!text.size) {
        return text.data ? ON1X_SDK_OK : ON1X_SDK_OK;
    }
    return on1x_sdk_identifier_validate(text);
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_diagnostic_validate(
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
) {
    const auto header = on1x_sdk_abi_header_validate(
        diagnostic, sizeof(On1xSDK_DiagnosticV2),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (header != ON1X_SDK_OK) {
        return header;
    }
    if (diagnostic->reserved ||
        diagnostic->severity < ON1X_SDK_NOTE ||
        diagnostic->severity > ON1X_SDK_DIAGNOSTIC_ERROR ||
        on1x_sdk_identifier_validate(diagnostic->code) != ON1X_SDK_OK ||
        contains_nul(diagnostic->code) || !diagnostic->message.size) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = on1x_sdk_string_validate_utf8(
        diagnostic->message, ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    status = on1x_sdk_span_validate(&diagnostic->primary, source, 1);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (diagnostic->related_count > ON1X_SDK_MAX_RELATED_DIAGNOSTICS ||
        diagnostic->fixit_count > ON1X_SDK_MAX_FIXITS) {
        return ON1X_SDK_LIMIT;
    }
    if ((!diagnostic->related && diagnostic->related_count) ||
        (!diagnostic->fixits && diagnostic->fixit_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    for (std::size_t i = 0; i < diagnostic->related_count; ++i) {
        const auto &related = diagnostic->related[i];
        if ((status = on1x_sdk_span_validate(&related.span, source, 0)) !=
                ON1X_SDK_OK ||
            !related.message.size) {
            return status == ON1X_SDK_OK
                ? ON1X_SDK_INVALID_ARGUMENT : status;
        }
        status = on1x_sdk_string_validate_utf8(
            related.message, ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
    }
    for (std::size_t i = 0; i < diagnostic->fixit_count; ++i) {
        const auto &fix = diagnostic->fixits[i];
        if (fix.is_proposal != 1 || fix.reserved ||
            (status = on1x_sdk_span_validate(&fix.span, source, 0)) !=
                ON1X_SDK_OK) {
            return status == ON1X_SDK_OK
                ? ON1X_SDK_INVALID_ARGUMENT : status;
        }
        status = on1x_sdk_string_validate_utf8(
            fix.replacement, ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
    }
    if (optional_identifier(diagnostic->category) != ON1X_SDK_OK ||
        optional_identifier(diagnostic->phase) != ON1X_SDK_OK ||
        optional_identifier(diagnostic->provider) != ON1X_SDK_OK) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_diagnostic_emit_v2(
    const On1xSDK_DiagnosticSinkV2 *sink,
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
) {
    const auto status = on1x_sdk_diagnostic_validate(diagnostic, source);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!sink) {
        return ON1X_SDK_OK;
    }
    const auto sink_status = on1x_sdk_abi_header_validate(
        sink, sizeof(On1xSDK_DiagnosticSinkV2),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (sink_status != ON1X_SDK_OK) {
        return sink_status;
    }
    if (!sink->emit) {
        return ON1X_SDK_OK;
    }
    try {
        return on1x::sdk::v2::normalize_status(
            sink->emit(sink->userdata, diagnostic)
        );
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

extern "C" On1xSDK_Status on1x_sdk_diagnostic_emit_v1(
    const On1xSDK_DiagnosticSink *sink,
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
) {
    const auto status = on1x_sdk_diagnostic_validate(diagnostic, source);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!sink || !sink->emit) {
        return ON1X_SDK_OK;
    }
    try {
        std::string code(diagnostic->code.data, diagnostic->code.size);
        std::string message(diagnostic->message.data, diagnostic->message.size);
        On1xSDK_Diagnostic record{};
        record.severity = diagnostic->severity;
        record.code = code.c_str();
        record.message = message.c_str();
        const bool located = diagnostic->primary.byte_begin != UINT64_MAX;
        if (located &&
            diagnostic->primary.byte_end <=
                std::numeric_limits<std::size_t>::max()) {
            record.byte_begin =
                static_cast<std::size_t>(diagnostic->primary.byte_begin);
            record.byte_end =
                static_cast<std::size_t>(diagnostic->primary.byte_end);
        } else {
            record.byte_begin = SIZE_MAX;
            record.byte_end = SIZE_MAX;
        }
        sink->emit(sink->userdata, &record);
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}
