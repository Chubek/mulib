#include "ProjectInternal.hpp"

namespace on1x::sdk::project {
namespace {

constexpr std::array<std::uint32_t, 64> sha_k{{
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
}};

constexpr std::uint32_t rotr(std::uint32_t value, unsigned count) noexcept {
    return (value >> count) | (value << (32 - count));
}

struct Sha {
    std::array<std::uint32_t, 8> state{{
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    }};
    std::array<unsigned char, 64> block{};
    std::size_t used = 0;
    std::uint64_t bytes = 0;

    void compress(const unsigned char* data) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i)
            words[i] = (std::uint32_t(data[i * 4]) << 24) |
                       (std::uint32_t(data[i * 4 + 1]) << 16) |
                       (std::uint32_t(data[i * 4 + 2]) << 8) |
                       std::uint32_t(data[i * 4 + 3]);
        for (std::size_t i = 16; i < words.size(); ++i) {
            const auto a = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const auto b = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + a + words[i - 7] + b;
        }
        auto [a,b,c,d,e,f,g,h] = state;
        for (std::size_t i = 0; i < words.size(); ++i) {
            const auto s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const auto choose = (e & f) ^ (~e & g);
            const auto t1 = h + s1 + choose + sha_k[i] + words[i];
            const auto s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto t2 = s0 + majority;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    void update(std::span<const unsigned char> input) noexcept {
        bytes += input.size();
        while (!input.empty()) {
            const auto count = std::min(block.size() - used, input.size());
            std::memcpy(block.data() + used, input.data(), count);
            used += count;
            input = input.subspan(count);
            if (used == block.size()) {
                compress(block.data());
                used = 0;
            }
        }
    }

    std::array<unsigned char, 32> finish() noexcept {
        const auto bits = bytes * 8;
        block[used++] = 0x80;
        if (used > 56) {
            std::fill(block.begin() + static_cast<std::ptrdiff_t>(used), block.end(), 0);
            compress(block.data());
            used = 0;
        }
        std::fill(block.begin() + static_cast<std::ptrdiff_t>(used), block.begin() + 56, 0);
        for (unsigned i = 0; i < 8; ++i)
            block[63 - i] = static_cast<unsigned char>(bits >> (i * 8));
        compress(block.data());
        std::array<unsigned char, 32> output{};
        for (std::size_t i = 0; i < state.size(); ++i) {
            output[i * 4] = static_cast<unsigned char>(state[i] >> 24);
            output[i * 4 + 1] = static_cast<unsigned char>(state[i] >> 16);
            output[i * 4 + 2] = static_cast<unsigned char>(state[i] >> 8);
            output[i * 4 + 3] = static_cast<unsigned char>(state[i]);
        }
        return output;
    }
};

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void append_utf8(std::string& output, std::uint32_t value, std::size_t at) {
    if (value <= 0x7f) {
        output.push_back(static_cast<char>(value));
    } else if (value <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (value >> 6)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (value >> 12)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value <= 0x10ffff) {
        output.push_back(static_cast<char>(0xf0 | (value >> 18)));
        output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
             "invalid Unicode scalar in quoted field", at, at + 1);
    }
}

std::uint32_t unicode_escape(std::string_view input, std::size_t& position,
                             std::size_t base) {
    if (position + 4 > input.size())
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
             "truncated Unicode escape", base + position, base + input.size());
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const int digit = hex_value(input[position++]);
        if (digit < 0)
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                 "invalid Unicode escape", base + position - 1, base + position);
        value = value * 16 + static_cast<unsigned>(digit);
    }
    return value;
}

Token quoted_token(std::string_view line, std::size_t& position, std::size_t base) {
    Token token;
    token.quoted = true;
    token.begin = base + position;
    const auto start = position++;
    while (position < line.size()) {
        const unsigned char c = static_cast<unsigned char>(line[position++]);
        if (c == '"') {
            token.end = base + position;
            token.spelling.assign(line.substr(start, position - start));
            if (!utf8(token.value))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-UTF8",
                     "quoted field is not valid UTF-8", token.begin, token.end);
            if (token.value.size() > 4096 || token.value.find('\0') != std::string::npos)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-MANIFEST-FIELD-LIMIT",
                     "quoted field exceeds its byte limit", token.begin, token.end);
            return token;
        }
        if (c < 0x20)
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                 "control byte in quoted field", base + position - 1, base + position);
        if (c != '\\') {
            token.value.push_back(static_cast<char>(c));
            continue;
        }
        if (position == line.size())
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                 "truncated quoted escape", base + position - 1, base + position);
        const char escaped = line[position++];
        switch (escaped) {
            case '"': case '\\': case '/': token.value.push_back(escaped); break;
            case 'b': token.value.push_back('\b'); break;
            case 'f': token.value.push_back('\f'); break;
            case 'n': token.value.push_back('\n'); break;
            case 'r': token.value.push_back('\r'); break;
            case 't': token.value.push_back('\t'); break;
            case 'u': {
                auto scalar = unicode_escape(line, position, base);
                if (scalar >= 0xd800 && scalar <= 0xdbff) {
                    if (position + 2 > line.size() || line[position] != '\\' ||
                        line[position + 1] != 'u')
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                             "high surrogate requires a low surrogate",
                             base + position - 4, base + position);
                    position += 2;
                    const auto low = unicode_escape(line, position, base);
                    if (low < 0xdc00 || low > 0xdfff)
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                             "invalid low surrogate", base + position - 4, base + position);
                    scalar = 0x10000 + ((scalar - 0xd800) << 10) + (low - 0xdc00);
                } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                         "unexpected low surrogate", base + position - 4, base + position);
                }
                append_utf8(token.value, scalar, base + position);
                break;
            }
            default:
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
                     "unknown quoted escape", base + position - 1, base + position);
        }
    }
    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-STRING",
         "unterminated quoted field", token.begin, base + line.size());
}

bool git_commit(std::string_view value) noexcept {
    if (value.size() != 40 && value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool git_ref(std::string_view value) noexcept {
    if (value.empty() || value.size() > 240 || value == "@" ||
        !ascii_alnum(static_cast<unsigned char>(value.front())) ||
        value.back() == '/' || value.back() == '.' ||
        value.find("//") != std::string_view::npos ||
        value.find("..") != std::string_view::npos ||
        value.find("@{") != std::string_view::npos)
        return false;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto slash = value.find('/', start);
        const auto end = slash == std::string_view::npos ? value.size() : slash;
        const auto part = value.substr(start, end - start);
        if (part.empty() || part.front() == '.' || part.ends_with(".lock")) return false;
        for (const unsigned char c : part)
            if (!ascii_alnum(c) && c != '.' && c != '_' && c != '+' && c != '-')
                return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}

bool remote_url(std::string_view value) noexcept {
    if (!value.starts_with("https://") || value.size() > 2048) return false;
    value.remove_prefix(8);
    const auto slash = value.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == value.size())
        return false;
    const auto authority = value.substr(0, slash);
    if (authority.find('@') != std::string_view::npos ||
        authority.find('[') != std::string_view::npos ||
        authority.find(']') != std::string_view::npos)
        return false;
    const auto colon = authority.rfind(':');
    const auto host = colon == std::string_view::npos
                          ? authority
                          : authority.substr(0, colon);
    if (host.empty() || host.size() > 253 || host.front() == '.' ||
        host.back() == '.' || host.find("..") != std::string_view::npos)
        return false;
    if (colon != std::string_view::npos) {
        const auto port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5 ||
            !std::all_of(port.begin(), port.end(),
                         [](char c) { return c >= '0' && c <= '9'; }))
            return false;
        unsigned port_number = 0;
        for (const char c : port)
            port_number = port_number * 10 + unsigned(c - '0');
        if (!port_number || port_number > 65535) return false;
    }
    std::size_t label_start = 0;
    while (label_start < host.size()) {
        const auto dot = host.find('.', label_start);
        const auto label_end =
            dot == std::string_view::npos ? host.size() : dot;
        const auto label = host.substr(label_start, label_end - label_start);
        if (label.empty() || label.size() > 63 ||
            !ascii_alnum(static_cast<unsigned char>(label.front())) ||
            !ascii_alnum(static_cast<unsigned char>(label.back())))
            return false;
        for (const unsigned char c : label)
            if (!ascii_alnum(c) && c != '-') return false;
        if (dot == std::string_view::npos) break;
        label_start = dot + 1;
    }
    std::size_t start = slash + 1;
    while (start <= value.size()) {
        const auto next = value.find('/', start);
        const auto end = next == std::string_view::npos ? value.size() : next;
        const auto part = value.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (const unsigned char c : part)
            if (!ascii_alnum(c) && c != '.' && c != '_' && c != '~' && c != '-')
                return false;
        if (next == std::string_view::npos) break;
        start = next + 1;
    }
    return true;
}

void exact(const Row& row, std::size_t count) {
    if (row.tokens.size() != count)
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-ARITY",
             "missing or extra manifest fields", row.line_begin, row.line_end);
}

void quoted(const Token& token, bool expected) {
    if (token.quoted != expected)
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-QUOTING",
             expected ? "manifest field must be quoted" : "manifest field must be a word",
             token.begin, token.end);
}

} // namespace

bool allocator_valid(const On1xSDK_ProjectAllocator* allocator) noexcept {
    constexpr auto required = offsetof(On1xSDK_ProjectAllocator, release) +
                              sizeof(On1xSDK_ProjectAllocator::release);
    return descriptor(allocator, required) && allocator->allocate && allocator->release;
}

On1xSDK_Status diagnostic_sink_status(
    const On1xSDK_ProjectDiagnosticSink* sink) noexcept {
    if (!sink) return ON1X_SDK_OK;
    constexpr auto required = offsetof(On1xSDK_ProjectDiagnosticSink, emit) +
                              sizeof(On1xSDK_ProjectDiagnosticSink::emit);
    if (sink->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
        return ON1X_SDK_ABI_MISMATCH;
    return descriptor(sink, required) && sink->emit
               ? ON1X_SDK_OK
               : ON1X_SDK_INVALID_ARGUMENT;
}

void* allocate(const On1xSDK_ProjectAllocator& allocator, std::size_t size) {
    if (!size) size = 1;
    void* result = nullptr;
    try {
        result = allocator.allocate(allocator.context, size, object_alignment);
    } catch (...) {
        result = nullptr;
    }
    if (!result)
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
             "host allocator rejected the project allocation");
    if (reinterpret_cast<std::uintptr_t>(result) % object_alignment != 0) {
        try {
            allocator.release(allocator.context, result, size, object_alignment);
        } catch (...) {}
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PROJECT-ALLOCATOR",
             "host allocator returned insufficiently aligned memory");
    }
    return result;
}

void release(const On1xSDK_ProjectAllocator& allocator, void* pointer,
             std::size_t size) noexcept {
    try {
        allocator.release(allocator.context, pointer, size, object_alignment);
    } catch (...) {}
}

Digest sha256(std::span<const unsigned char> input) noexcept {
    Sha state;
    state.update(input);
    Digest result;
    result.bytes = state.finish();
    static constexpr char alphabet[] = "0123456789abcdef";
    for (std::size_t i = 0; i < result.bytes.size(); ++i) {
        result.hex[i * 2] = alphabet[result.bytes[i] >> 4];
        result.hex[i * 2 + 1] = alphabet[result.bytes[i] & 15];
    }
    result.hex.back() = '\0';
    return result;
}

bool sha256_text(std::string_view input) noexcept {
    return input.size() == ON1X_SDK_PROJECT_SHA256_TEXT_BYTES &&
           std::all_of(input.begin(), input.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

bool utf8(std::string_view input) noexcept {
    for (std::size_t i = 0; i < input.size();) {
        const auto first = static_cast<unsigned char>(input[i++]);
        if (first <= 0x7f) {
            if (first == 0) return false;
            continue;
        }
        unsigned count = 0;
        std::uint32_t value = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 1; value = first & 0x1f; }
        else if (first >= 0xe0 && first <= 0xef) { count = 2; value = first & 0x0f; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 3; value = first & 0x07; }
        else return false;
        if (i + count > input.size()) return false;
        for (unsigned j = 0; j < count; ++j) {
            const auto next = static_cast<unsigned char>(input[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 0x3f);
        }
        if ((count == 2 && value < 0x800) || (count == 3 && value < 0x10000) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}

bool normalized_path(std::string_view input, bool allow_root_dot) noexcept {
    if (input == ".") return allow_root_dot;
    if (input.empty() || input.size() > ON1X_SDK_PROJECT_MAX_PATH_BYTES ||
        input.front() == '/' || input.back() == '/' ||
        input.find('\\') != std::string_view::npos)
        return false;
    std::size_t parts = 0;
    for (std::size_t start = 0; start < input.size();) {
        const auto slash = input.find('/', start);
        const auto end = slash == std::string_view::npos ? input.size() : slash;
        const auto part = input.substr(start, end - start);
        if (++parts > 32 || part.empty() || part == "." || part == ".." ||
            part.size() > 128)
            return false;
        const auto first = static_cast<unsigned char>(part.front());
        if (!ascii_alnum(first) && first != '_') return false;
        for (const unsigned char c : part)
            if (!ascii_alnum(c) && c != '_' && c != '.' && c != ' ' && c != '-')
                return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}

bool absolute_path(std::string_view input) noexcept {
    if (input.empty() || input.size() > 16384 || input.front() != '/' ||
        input.find('\0') != std::string_view::npos ||
        input.find('\\') != std::string_view::npos)
        return false;
    for (const unsigned char c : input) if (c < 0x20 || c == 0x7f) return false;
    std::size_t start = 1;
    while (start < input.size()) {
        const auto slash = input.find('/', start);
        const auto end = slash == std::string_view::npos ? input.size() : slash;
        const auto part = input.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return utf8(input);
}

bool simple_name(std::string_view input) noexcept {
    if (input.empty() || input.size() > 64 ||
        !((input.front() >= 'A' && input.front() <= 'Z') ||
          (input.front() >= 'a' && input.front() <= 'z') || input.front() == '_'))
        return false;
    return std::all_of(input.begin() + 1, input.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

bool identifier_name(std::string_view input) noexcept {
    if (!simple_name(input)) return false;
    return input.find('-') == std::string_view::npos;
}

bool command_component(std::string_view input) noexcept {
    if (input.empty() || input.size() > 64 || input.front() < 'a' || input.front() > 'z')
        return false;
    bool hyphen = false;
    for (char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) hyphen = false;
        else if (c == '-' && !hyphen) hyphen = true;
        else return false;
    }
    return !hyphen;
}

On1xSDK_Status callback_status(On1xSDK_Status status) noexcept {
    switch (status) {
        case ON1X_SDK_OK: case ON1X_SDK_ERROR: case ON1X_SDK_UNSUPPORTED:
        case ON1X_SDK_LIMIT: case ON1X_SDK_INVALID_ARGUMENT:
        case ON1X_SDK_ABI_MISMATCH: case ON1X_SDK_DUPLICATE:
        case ON1X_SDK_BUSY:
            return status;
        default:
            return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status emit(const On1xSDK_ProjectDiagnosticSink* sink,
                    const Failure& failure, const Digest& revision,
                    std::size_t input_size) noexcept {
    if (!sink) return failure.status;
    const auto sink_status = diagnostic_sink_status(sink);
    if (sink_status != ON1X_SDK_OK) return sink_status;
    On1xSDK_ProjectDiagnostic diagnostic{};
    diagnostic.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.code = view(failure.code);
    diagnostic.severity = ON1X_SDK_PROJECT_ERROR;
    diagnostic.message = view(failure.message);
    diagnostic.primary.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    diagnostic.primary.struct_size = sizeof(diagnostic.primary);
    std::copy(revision.bytes.begin(), revision.bytes.end(),
              diagnostic.primary.source_revision);
    diagnostic.primary.begin = std::min(failure.begin, input_size);
    diagnostic.primary.end = std::clamp(failure.end, diagnostic.primary.begin, input_size);
    On1xSDK_Status status = ON1X_SDK_ERROR;
    try {
        status = callback_status(sink->emit(sink->context, &diagnostic));
    } catch (...) {
        status = ON1X_SDK_ERROR;
    }
    return status == ON1X_SDK_OK ? failure.status : status;
}

On1xSDK_ProjectBuffer* make_buffer(const On1xSDK_ProjectAllocator& allocator,
                                   std::string_view bytes) {
    std::size_t size = aligned(sizeof(On1xSDK_ProjectBuffer), alignof(unsigned char));
    if (!checked_add(size, bytes.size()))
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "buffer size overflow");
    void* memory = allocate(allocator, size);
    Blob blob(memory, size);
    auto* result = blob.array<On1xSDK_ProjectBuffer>();
    auto* data = blob.array<unsigned char>(bytes.size());
    if (!bytes.empty()) std::memcpy(data, bytes.data(), bytes.size());
    result->magic = buffer_magic;
    result->allocator = allocator;
    result->allocation_size = size;
    result->size = bytes.size();
    result->data = bytes.empty() ? nullptr : data;
    return result;
}

std::vector<Row> lex_manifest(std::string_view input) {
    if (!utf8(input))
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-UTF8",
             "manifest must be valid NUL-free UTF-8", 0, input.size());
    std::vector<Row> result;
    for (std::size_t begin = 0; begin <= input.size();) {
        const auto newline = input.find('\n', begin);
        const auto end = newline == std::string_view::npos ? input.size() : newline;
        auto line = input.substr(begin, end - begin);
        Row row{begin, end, {}, {}};
        std::size_t position = 0;
        while (position < line.size()) {
            while (position < line.size() &&
                   (line[position] == ' ' || line[position] == '\t' || line[position] == '\r'))
                ++position;
            if (position == line.size()) break;
            if (line[position] == '#') {
                row.comment.assign(line.substr(position));
                break;
            }
            if (row.tokens.size() >= 16)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-MANIFEST-FIELD-LIMIT",
                     "too many fields on one manifest line", begin, end);
            if (line[position] == '"') {
                row.tokens.push_back(quoted_token(line, position, begin));
            } else {
                Token token;
                token.begin = begin + position;
                while (position < line.size()) {
                    const unsigned char c = static_cast<unsigned char>(line[position]);
                    if (!ascii_alnum(c) && c != '_' && c != '-') break;
                    ++position;
                }
                if (token.begin == begin + position)
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-TOKEN",
                         "expected a directive word or quoted field",
                         begin + position, begin + position + 1);
                token.end = begin + position;
                token.value.assign(input.substr(token.begin, token.end - token.begin));
                token.spelling = token.value;
                row.tokens.push_back(std::move(token));
            }
            if (position < line.size() && line[position] != ' ' && line[position] != '\t' &&
                line[position] != '\r' && line[position] != '#')
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-TOKEN",
                     "manifest fields must be separated", begin + position, begin + position + 1);
        }
        result.push_back(std::move(row));
        if (newline == std::string_view::npos) break;
        begin = newline + 1;
    }
    return result;
}

ManifestTemp parse_manifest_temp(std::string_view input, std::size_t max_targets,
                                 std::size_t max_dependencies,
                                 std::vector<Row>* output_rows) {
    auto rows = lex_manifest(input);
    ManifestTemp result;
    bool header = false;
    bool have_kind = false, have_name = false, have_directory = false;
    std::set<std::string> targets, dependencies;
    for (const auto& row : rows) {
        if (row.tokens.empty()) continue;
        if (!header) {
            exact(row, 2);
            quoted(row.tokens[0], false);
            quoted(row.tokens[1], false);
            if (row.tokens[0].value != "on1xfile" || row.tokens[1].value != "1")
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-VERSION",
                     "expected on1xfile 1", row.line_begin, row.line_end);
            header = true;
            continue;
        }
        const auto& verb = row.tokens.front();
        quoted(verb, false);
        if (verb.value == "kind") {
            exact(row, 2); quoted(row.tokens[1], false);
            if (have_kind)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-MANIFEST-DUPLICATE",
                     "duplicate kind directive", verb.begin, verb.end);
            have_kind = true;
            if (row.tokens[1].value == "executable") result.kind = ON1X_SDK_PROJECT_EXECUTABLE;
            else if (row.tokens[1].value == "complex") result.kind = ON1X_SDK_PROJECT_COMPLEX;
            else if (row.tokens[1].value == "library") result.kind = ON1X_SDK_PROJECT_LIBRARY;
            else fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-MANIFEST-KIND",
                      "unsupported project kind", row.tokens[1].begin, row.tokens[1].end);
        } else if (verb.value == "name") {
            exact(row, 2); quoted(row.tokens[1], true);
            if (have_name)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-MANIFEST-DUPLICATE",
                     "duplicate name directive", verb.begin, verb.end);
            if (!simple_name(row.tokens[1].value))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-NAME",
                     "invalid project name", row.tokens[1].begin, row.tokens[1].end);
            result.name = row.tokens[1].value;
            have_name = true;
        } else if (verb.value == "directory") {
            exact(row, 2); quoted(row.tokens[1], true);
            if (have_directory)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-MANIFEST-DUPLICATE",
                     "duplicate directory directive", verb.begin, verb.end);
            if (!normalized_path(row.tokens[1].value, true))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PATH",
                     "directory must be a normalized local path",
                     row.tokens[1].begin, row.tokens[1].end);
            result.directory = row.tokens[1].value;
            have_directory = true;
        } else if (verb.value == "target") {
            exact(row, 3); quoted(row.tokens[1], true); quoted(row.tokens[2], true);
            if (result.targets.size() >= max_targets)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-MANIFEST-TARGET-LIMIT",
                     "target count limit exceeded", verb.begin, row.line_end);
            if (!simple_name(row.tokens[1].value))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-NAME",
                     "invalid target name", row.tokens[1].begin, row.tokens[1].end);
            if (!normalized_path(row.tokens[2].value, false))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PATH",
                     "target source must be a normalized local path",
                     row.tokens[2].begin, row.tokens[2].end);
            if (!targets.insert(row.tokens[1].value).second)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-MANIFEST-TARGET-DUPLICATE",
                     "duplicate target name", row.tokens[1].begin, row.tokens[1].end);
            result.targets.push_back({row.tokens[1].value, row.tokens[2].value});
        } else if (verb.value == "dependency") {
            if (result.dependencies.size() >= max_dependencies)
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-MANIFEST-DEPENDENCY-LIMIT",
                     "dependency count limit exceeded", verb.begin, row.line_end);
            if (row.tokens.size() != 4 && row.tokens.size() != 5 &&
                row.tokens.size() != 6)
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-ARITY",
                     "invalid dependency declaration", row.line_begin, row.line_end);
            quoted(row.tokens[1], true); quoted(row.tokens[2], false); quoted(row.tokens[3], true);
            if (!identifier_name(row.tokens[1].value))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-NAME",
                     "invalid dependency name", row.tokens[1].begin, row.tokens[1].end);
            if (!dependencies.insert(row.tokens[1].value).second)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-MANIFEST-DEPENDENCY-DUPLICATE",
                     "duplicate dependency name", row.tokens[1].begin, row.tokens[1].end);
            DependencyTemp dependency;
            dependency.name = row.tokens[1].value;
            dependency.source = row.tokens[3].value;
            if (row.tokens[2].value == "local") {
                if (row.tokens.size() != 4 || !normalized_path(dependency.source, false))
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PATH",
                         "local dependency requires a normalized local path",
                         row.tokens[3].begin, row.line_end);
                dependency.kind = ON1X_SDK_DEPENDENCY_LOCAL;
                dependency.selector_kind = ON1X_SDK_SELECTOR_NONE;
            } else {
                dependency.kind = row.tokens[2].value == "git" ? ON1X_SDK_DEPENDENCY_GIT :
                                  row.tokens[2].value == "git-remote" ?
                                      ON1X_SDK_DEPENDENCY_GIT_REMOTE : 0;
                if (!dependency.kind)
                    fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-MANIFEST-DEPENDENCY-KIND",
                         "dependency kind must be local, git or git-remote",
                         row.tokens[2].begin, row.tokens[2].end);
                if ((dependency.kind == ON1X_SDK_DEPENDENCY_GIT &&
                     !normalized_path(dependency.source, false)) ||
                    (dependency.kind == ON1X_SDK_DEPENDENCY_GIT_REMOTE &&
                     !remote_url(dependency.source)))
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-DEPENDENCY-SOURCE",
                         "invalid Git dependency source", row.tokens[3].begin, row.tokens[3].end);
                if (row.tokens.size() == 4)
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-ARITY",
                         "Git dependency requires a selector", row.line_begin, row.line_end);
                if (row.tokens.size() == 5) {
                    quoted(row.tokens[4], true);
                    dependency.selector = row.tokens[4].value;
                } else {
                    quoted(row.tokens[4], false);
                    quoted(row.tokens[5], true);
                    dependency.selector = row.tokens[5].value;
                }
                if (row.tokens.size() == 5 && git_commit(dependency.selector)) {
                    dependency.selector_kind = ON1X_SDK_SELECTOR_COMMIT;
                } else if (row.tokens.size() == 6 &&
                           row.tokens[4].value == "commit" &&
                           git_commit(dependency.selector)) {
                    dependency.selector_kind = ON1X_SDK_SELECTOR_COMMIT;
                } else if (row.tokens.size() == 6 &&
                           row.tokens[4].value == "branch" &&
                           git_ref(dependency.selector)) {
                    dependency.selector_kind = ON1X_SDK_SELECTOR_BRANCH;
                } else if (row.tokens.size() == 6 &&
                           row.tokens[4].value == "tag" &&
                           git_ref(dependency.selector)) {
                    dependency.selector_kind = ON1X_SDK_SELECTOR_TAG;
                } else {
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-SELECTOR",
                         "Git selector must be a full commit or valid commit, branch or tag selector",
                         row.tokens[4].begin, row.tokens.back().end);
                }
            }
            result.dependencies.push_back(std::move(dependency));
        } else {
            fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-MANIFEST-DIRECTIVE",
                 "directive is outside the reusable project SDK subset",
                 verb.begin, verb.end);
        }
    }
    if (!header)
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-VERSION",
             "missing on1xfile 1 header", 0, input.size());
    if (!have_kind || !have_name)
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-REQUIRED",
             "manifest requires kind and name", 0, input.size());
    const auto targets_count = result.targets.size();
    if ((result.kind == ON1X_SDK_PROJECT_EXECUTABLE && targets_count != 1) ||
        (result.kind == ON1X_SDK_PROJECT_COMPLEX && targets_count == 0) ||
        (result.kind == ON1X_SDK_PROJECT_LIBRARY && targets_count != 0))
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-MANIFEST-TARGETS",
             "executable needs one target, complex needs at least one, and library allows none",
             0, input.size());
    if (output_rows) *output_rows = std::move(rows);
    return result;
}

} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_project_normalize_path(
    On1xSDK_ProjectString input, int allow_root_dot, char* output,
    size_t output_capacity, size_t* out_size) {
    using namespace on1x::sdk::project;
    if (!out_size || !valid_view(input) || (output_capacity && !output))
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_size = input.size;
    const auto source = as_view(input);
    if (!normalized_path(source, allow_root_dot != 0)) return ON1X_SDK_INVALID_ARGUMENT;
    if (output_capacity < source.size()) return ON1X_SDK_LIMIT;
    if (!source.empty()) std::memmove(output, source.data(), source.size());
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_project_sha256(
    On1xSDK_ProjectBytes input,
    unsigned char output[ON1X_SDK_PROJECT_SHA256_BYTES]) {
    using namespace on1x::sdk::project;
    if (!output || !valid_view(input)) return ON1X_SDK_INVALID_ARGUMENT;
    const auto digest = sha256(as_span(input));
    std::copy(digest.bytes.begin(), digest.bytes.end(), output);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_ProjectBytes on1x_sdk_project_buffer_view(
    const On1xSDK_ProjectBuffer* buffer) {
    using namespace on1x::sdk::project;
    if (!buffer || buffer->magic != buffer_magic) return {};
    return {buffer->data, buffer->size};
}

extern "C" void on1x_sdk_project_buffer_destroy(On1xSDK_ProjectBuffer* buffer) {
    using namespace on1x::sdk::project;
    if (!buffer || buffer->magic != buffer_magic) return;
    const auto allocator = buffer->allocator;
    const auto size = buffer->allocation_size;
    buffer->magic = 0;
    release(allocator, buffer, size);
}
