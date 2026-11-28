#include "ProjectInternal.hpp"

namespace on1x::sdk::project {
namespace {

std::string json_string(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 2);
    output.push_back('"');
    static constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : value) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20) {
                    output += "\\u00";
                    output.push_back(hex[c >> 4]);
                    output.push_back(hex[c & 15]);
                } else {
                    output.push_back(static_cast<char>(c));
                }
        }
    }
    output.push_back('"');
    return output;
}

std::string format_rows(const std::vector<Row>& rows) {
    std::vector<std::string> lines;
    lines.reserve(rows.size());
    for (const auto& row : rows) {
        std::string line;
        for (const auto& token : row.tokens) {
            if (!line.empty()) line.push_back(' ');
            line += token.quoted ? json_string(token.value) : token.value;
        }
        std::string comment = row.comment;
        while (!comment.empty() &&
               (comment.back() == ' ' || comment.back() == '\t' ||
                comment.back() == '\r'))
            comment.pop_back();
        if (!comment.empty()) {
            if (!line.empty()) line.push_back(' ');
            line += comment;
        }
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    std::string output;
    for (const auto& line : lines) {
        output += line;
        output.push_back('\n');
    }
    if (output.empty()) output.push_back('\n');
    return output;
}

bool utf8_boundary(std::span<const unsigned char> input, std::size_t offset) noexcept {
    return offset == 0 || offset == input.size() ||
           (input[offset] & 0xc0) != 0x80;
}

} // namespace
} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_manifest_format(
    On1xSDK_ProjectBytes input, const On1xSDK_ProjectAllocator* allocator,
    const On1xSDK_ProjectDiagnosticSink* sink,
    On1xSDK_ProjectBuffer** output) {
    using namespace on1x::sdk::project;
    if (!output) return ON1X_SDK_INVALID_ARGUMENT;
    *output = nullptr;
    if (!valid_view(input) || !allocator || !allocator_valid(allocator)) {
        if (allocator && allocator->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = diagnostic_sink_status(sink);
        status != ON1X_SDK_OK)
        return status;
    if (input.size > ON1X_SDK_MANIFEST_MAX_BYTES)
        return ON1X_SDK_LIMIT;
    const auto revision = sha256(as_span(input));
    try {
        const auto text =
            input.size
                ? std::string_view(reinterpret_cast<const char*>(input.data),
                                   input.size)
                : std::string_view{};
        std::vector<Row> rows;
        (void)parse_manifest_temp(text, ON1X_SDK_MANIFEST_MAX_TARGETS,
                                  ON1X_SDK_MANIFEST_MAX_DEPENDENCIES, &rows);
        const auto formatted = format_rows(rows);
        *output = make_buffer(*allocator, formatted);
        return ON1X_SDK_OK;
    } catch (const Failure& failure) {
        return emit(sink, failure, revision, input.size);
    } catch (const std::bad_alloc&) {
        return emit(sink, {ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
                           "manifest formatting allocation failed", 0, input.size},
                    revision, input.size);
    } catch (...) {
        return emit(sink, {ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                           "unexpected manifest formatter failure", 0, input.size},
                    revision, input.size);
    }
}

extern "C" On1xSDK_Status on1x_sdk_code_action_apply_candidate(
    On1xSDK_ProjectBytes source, On1xSDK_ProjectString expected_revision,
    const On1xSDK_CodeActionEdit* edits, size_t edit_count,
    const On1xSDK_ProjectAllocator* allocator,
    const On1xSDK_ProjectDiagnosticSink* sink,
    On1xSDK_ProjectBuffer** output) {
    using namespace on1x::sdk::project;
    if (!output) return ON1X_SDK_INVALID_ARGUMENT;
    *output = nullptr;
    if (!valid_view(source) || !valid_view(expected_revision) ||
        (edit_count && !edits) || !allocator || !allocator_valid(allocator)) {
        if (allocator && allocator->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = diagnostic_sink_status(sink);
        status != ON1X_SDK_OK)
        return status;
    if (source.size > ON1X_SDK_CODE_ACTION_MAX_OUTPUT ||
        edit_count > ON1X_SDK_CODE_ACTION_MAX_EDITS)
        return ON1X_SDK_LIMIT;
    const auto revision = sha256(as_span(source));
    try {
        const auto source_text =
            source.size
                ? std::string_view(
                      reinterpret_cast<const char*>(source.data), source.size)
                : std::string_view{};
        if (!utf8(source_text))
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-CODE-ACTION-UTF8",
                 "code-action source must be valid NUL-free UTF-8", 0,
                 source.size);
        if (!sha256_text(as_view(expected_revision)) ||
            as_view(expected_revision) !=
                std::string_view(revision.hex.data(),
                                 ON1X_SDK_PROJECT_SHA256_TEXT_BYTES))
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-CODE-ACTION-STALE",
                 "code action does not match the exact source revision", 0,
                 source.size);

        std::size_t output_size = source.size;
        std::size_t cursor = 0;
        std::size_t previous_begin = std::numeric_limits<std::size_t>::max();
        for (std::size_t i = 0; i < edit_count; ++i) {
            constexpr auto required =
                offsetof(On1xSDK_CodeActionEdit, replacement) +
                sizeof(On1xSDK_CodeActionEdit::replacement);
            const auto& edit = edits[i];
            if (!on1x::sdk::project::descriptor(&edit, required))
                fail(edit.abi_version == ON1X_SDK_PROJECT_ABI_VERSION
                         ? ON1X_SDK_INVALID_ARGUMENT : ON1X_SDK_ABI_MISMATCH,
                     "ON1X-SDK-CODE-ACTION-DESCRIPTOR",
                     "invalid code-action edit descriptor");
            if (!valid_view(edit.replacement) || edit.begin > edit.end ||
                edit.end > source.size || edit.begin < cursor ||
                (i && edit.begin == previous_begin))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-CODE-ACTION-SPAN",
                     "code-action edits must be ordered, unique and nonoverlapping",
                     std::min(edit.begin, source.size),
                     std::min(edit.end, source.size));
            if (edit.replacement.size > ON1X_SDK_CODE_ACTION_MAX_OUTPUT)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-CODE-ACTION-LIMIT",
                     "code-action replacement exceeds the output byte limit",
                     edit.begin, edit.end);
            if (!utf8(as_view(edit.replacement)) ||
                !utf8_boundary(as_span(source), edit.begin) ||
                !utf8_boundary(as_span(source), edit.end))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-CODE-ACTION-SPAN",
                     "code-action edits must be ordered, unique, nonoverlapping UTF-8 spans",
                     std::min(edit.begin, source.size),
                     std::min(edit.end, source.size));
            output_size -= edit.end - edit.begin;
            if (!checked_add(output_size, edit.replacement.size) ||
                output_size > ON1X_SDK_CODE_ACTION_MAX_OUTPUT)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-CODE-ACTION-LIMIT",
                     "code-action candidate exceeds the output byte limit",
                     edit.begin, edit.end);
            cursor = edit.end;
            previous_begin = edit.begin;
        }
        std::string candidate;
        candidate.reserve(output_size);
        cursor = 0;
        const auto text = source_text;
        for (std::size_t i = 0; i < edit_count; ++i) {
            candidate.append(text.substr(cursor, edits[i].begin - cursor));
            candidate.append(as_view(edits[i].replacement));
            cursor = edits[i].end;
        }
        candidate.append(text.substr(cursor));
        if (candidate.size() != output_size)
            fail(ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                 "code-action output accounting mismatch");
        *output = make_buffer(*allocator, candidate);
        return ON1X_SDK_OK;
    } catch (const Failure& failure) {
        return emit(sink, failure, revision, source.size);
    } catch (const std::bad_alloc&) {
        return emit(sink, {ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
                           "code-action allocation failed", 0, source.size},
                    revision, source.size);
    } catch (...) {
        return emit(sink, {ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                           "unexpected code-action failure", 0, source.size},
                    revision, source.size);
    }
}
