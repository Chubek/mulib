#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

using Registry = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;
using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;
using Artifact = std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>;
using Target = std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>;

constexpr std::string_view mir =
    "(on1x.mir 1 1 ((function 2 \"main\" i64 (at \"main.on1x\" 0 1 1) () "
    "((block 3 4 (at \"main.on1x\" 0 1 1) () "
    "((instruction 5 const i64 () 42 0 0 0 (at \"main.on1x\" 26 1 27))) "
    "(exit 6 return 5 4 () (at \"main.on1x\" 19 1 20)))))))\n";

Registry registry() {
    On1xSDK_Registry *raw = nullptr;
    CHECK(on1x_sdk_registry_create(&raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_registry_destroy};
}

Program verified(std::string_view wire = mir) {
    On1xSDK_Program *raw = nullptr;
    CHECK(on1x::sdk::make_verified_mir(wire, &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_program_destroy};
}

Program source() {
    On1xSDK_Program *raw = nullptr;
    CHECK(on1x_sdk_program_create_source(mir.data(), mir.size(), &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_program_destroy};
}

Target target(bool avx2) {
    const On1xSDK_TargetFeature feature{"avx2", 4};
    const auto layout = On1xSDK_DataLayout{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
        ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, 128};
    const char triple[] = "x86_64-unknown-linux-gnu";
    const On1xSDK_TargetDescriptor descriptor{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
        triple, sizeof(triple) - 1, nullptr, 0, layout,
        ON1X_SDK_OBJECT_FORMAT_ELF, ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64,
        avx2 ? &feature : nullptr, avx2 ? 1u : 0u};
    On1xSDK_Target *raw = nullptr;
    CHECK(on1x_sdk_target_create(&descriptor, &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_target_destroy};
}

enum class Mode {
    Good,
    Zero,
    Partial,
    Overrun,
    Throw,
    Unknown,
    V2Status,
    Flaky,
    Reenter
};

struct State {
    Mode mode{Mode::Good};
    std::string payload;
    std::size_t calls{};
    const On1xSDK_Registry *registry{};
    const On1xSDK_BackendRequest *request{};
    On1xSDK_Status nested{ON1X_SDK_OK};
};

extern "C" On1xSDK_Status run(void *userdata,
                               const On1xSDK_BackendInvocation *invocation,
                               On1xSDK_OutputSink *output) {
    auto &state = *static_cast<State *>(userdata);
    ++state.calls;
    CHECK(invocation);
    CHECK(invocation->input_schema == 1);
    CHECK(invocation->artifact_kind == ON1X_SDK_ARTIFACT_MIR_TEXT);
    if (const auto status = on1x_sdk_backend_poll(invocation);
        status != ON1X_SDK_OK)
        return status;
    switch (state.mode) {
    case Mode::Good:
        return on1x_sdk_output_write(output, state.payload.data(), state.payload.size());
    case Mode::Zero:
        return ON1X_SDK_OK;
    case Mode::Partial:
        CHECK(on1x_sdk_output_write(output, "partial", 7) == ON1X_SDK_OK);
        return ON1X_SDK_UNSUPPORTED;
    case Mode::Overrun:
        CHECK(on1x_sdk_output_write(output, "x", SIZE_MAX) == ON1X_SDK_LIMIT);
        return ON1X_SDK_OK;
    case Mode::Throw:
        throw 9;
    case Mode::Unknown:
        return static_cast<On1xSDK_Status>(200);
    case Mode::V2Status:
        return static_cast<On1xSDK_Status>(8);
    case Mode::Flaky: {
        const char value = state.calls & 1u ? 'a' : 'b';
        return on1x_sdk_output_write(output, &value, 1);
    }
    case Mode::Reenter: {
        On1xSDK_Artifact *nested = reinterpret_cast<On1xSDK_Artifact *>(1);
        state.nested = on1x_sdk_backend_produce(state.registry, state.request, &nested);
        CHECK(nested == nullptr);
        return on1x_sdk_output_write(output, "outer", 5);
    }
    }
    return ON1X_SDK_ERROR;
}

On1xSDK_BackendCapability independent_capability() {
    return {ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
            sizeof(On1xSDK_BackendCapability),
            ON1X_SDK_VERIFIED_MIR, 1, 1, ON1X_SDK_ARTIFACT_MIR_TEXT,
            ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
            nullptr, 0, nullptr, 0, nullptr, 0};
}

On1xSDK_Status add_provider(On1xSDK_Registry *registry, const char *name,
                            int priority, State &state,
                            On1xSDK_BackendCapability &capability) {
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        name, std::strlen(name), priority, 0, &capability, 1, run, &state};
    return on1x_sdk_backend_provider_register(registry, &provider);
}

On1xSDK_BackendRequest request(On1xSDK_Program *program) {
    return {ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
            sizeof(On1xSDK_BackendRequest), program, nullptr,
            ON1X_SDK_ARTIFACT_MIR_TEXT, 0, nullptr, 0, 0, 0,
            ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr};
}

struct Poll {
    std::size_t calls{};
    std::size_t stop{};
    On1xSDK_Status status{ON1X_SDK_BUSY};
};

extern "C" On1xSDK_Status poll(void *userdata) {
    auto &state = *static_cast<Poll *>(userdata);
    ++state.calls;
    return state.calls == state.stop ? state.status : ON1X_SDK_OK;
}

std::size_t null_poll_calls{};

extern "C" On1xSDK_Status null_poll(void *userdata) {
    CHECK(userdata == nullptr);
    ++null_poll_calls;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status no_userdata_run(
    void *userdata, const On1xSDK_BackendInvocation *invocation,
    On1xSDK_OutputSink *output) {
    CHECK(userdata == nullptr);
    CHECK(invocation && invocation->input_schema == 1);
    if (const auto status = on1x_sdk_backend_poll(invocation);
        status != ON1X_SDK_OK)
        return status;
    return on1x_sdk_output_write(output, "short", 5);
}

extern "C" On1xSDK_Status legacy_v2_status(
    void *, const On1xSDK_Program *, On1xSDK_OutputSink *) {
    return static_cast<On1xSDK_Status>(8);
}

std::string bytes(const On1xSDK_Artifact *artifact) {
    const uint8_t *data = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact, &data, &size) == ON1X_SDK_OK);
    return size ? std::string(reinterpret_cast<const char *>(data), size) : std::string{};
}

} // namespace

int main() {
    auto owner = registry();
    auto program = verified();
    auto capability = independent_capability();
    State low{Mode::Good, "low"};
    State high{Mode::Good, "abc"};
    char high_name[] = "provider.high";
    CHECK(add_provider(owner.get(), "provider.low", 1, low, capability) == ON1X_SDK_OK);
    CHECK(add_provider(owner.get(), high_name, 2, high, capability) == ON1X_SDK_OK);
    CHECK(add_provider(owner.get(), high_name, 2, high, capability) == ON1X_SDK_DUPLICATE);
    high_name[0] = 'x'; /* Registry owns the copied name. */

    auto invoke = request(program.get());
    On1xSDK_Artifact *raw = nullptr;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_OK);
    Artifact staged(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(staged.get()) == "abc");
    const On1xSDK_ArtifactInfo *info = nullptr;
    CHECK(on1x_sdk_artifact_info(staged.get(), &info) == ON1X_SDK_OK);
    CHECK(info && info->state == ON1X_SDK_ARTIFACT_STAGED &&
          info->kind == ON1X_SDK_ARTIFACT_MIR_TEXT && info->input_schema == 1 &&
          std::string(info->provider_name, info->provider_name_size) == "provider.high");
    const std::array<uint8_t, 32> abc_sha{
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    CHECK(std::equal(abc_sha.begin(), abc_sha.end(), info->sha256));

    invoke.provider_name = "provider.low";
    invoke.provider_name_size = 12;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_OK);
    Artifact low_artifact(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(low_artifact.get()) == "low");

    State tie{Mode::Good, "tie"};
    CHECK(add_provider(owner.get(), "provider.tie", 2, tie, capability) == ON1X_SDK_OK);
    invoke.provider_name = nullptr;
    invoke.provider_name_size = 0;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_DUPLICATE);
    CHECK(raw == nullptr);
    invoke.provider_name = "provider.high";
    invoke.provider_name_size = 13;

    invoke.input_schema = 2;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) ==
          ON1X_SDK_INVALID_ARGUMENT);
    invoke.input_schema = 0;
    invoke.artifact_kind = ON1X_SDK_ARTIFACT_OBJECT;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    invoke.artifact_kind = ON1X_SDK_ARTIFACT_MIR_TEXT;
    invoke.byte_limit = 2;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_LIMIT);
    invoke.byte_limit = ON1X_SDK_MAX_ARTIFACT_BYTES;

    high.mode = Mode::Partial;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(raw == nullptr);
    high.mode = Mode::Overrun;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_LIMIT);
    high.mode = Mode::Throw;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_ERROR);
    high.mode = Mode::Unknown;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_ERROR);
    high.mode = Mode::V2Status;
    CHECK(on1x_sdk_backend_run(owner.get(), "provider.high", program.get(),
                               ON1X_SDK_MAX_ARTIFACT_BYTES, &raw) ==
          ON1X_SDK_ERROR);
    CHECK(raw == nullptr);

    const auto cancelled_status = static_cast<On1xSDK_Status>(8);
    Poll cancelled{0, 1, cancelled_status};
    high.mode = Mode::Good;
    const auto calls = high.calls;
    invoke.poll = poll;
    invoke.poll_userdata = &cancelled;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) ==
          cancelled_status);
    CHECK(high.calls == calls);
    cancelled = {0, 3, ON1X_SDK_BUSY}; /* Pre, provider poll, post. */
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_BUSY);
    CHECK(raw == nullptr);
    invoke.poll = nullptr;
    invoke.poll_userdata = nullptr;

    high.mode = Mode::Good;
    invoke.flags = ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY;
    const auto before_repeat = high.calls;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_OK);
    Artifact repeated(raw, on1x_sdk_artifact_destroy);
    CHECK(high.calls == before_repeat + 2);
    CHECK(on1x_sdk_artifact_info(repeated.get(), &info) == ON1X_SDK_OK);
    CHECK(info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED);
    high.mode = Mode::Flaky;
    high.calls = 0;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_ERROR);
    CHECK(raw == nullptr);

    high.mode = Mode::Reenter;
    invoke.flags = 0;
    high.registry = owner.get();
    high.request = &invoke;
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, &raw) == ON1X_SDK_OK);
    Artifact outer(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(outer.get()) == "outer" && high.nested == ON1X_SDK_BUSY);

    auto unverified = source();
    auto bad_request = request(unverified.get());
    CHECK(on1x_sdk_backend_produce(owner.get(), &bad_request, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    auto malformed = verified("not mir");
    bad_request = request(malformed.get());
    CHECK(on1x_sdk_backend_produce(owner.get(), &bad_request, &raw) ==
          ON1X_SDK_INVALID_ARGUMENT);

    auto exact_registry = registry();
    auto avx = target(true);
    auto plain = target(false);
    const On1xSDK_BackendTarget target_constraint{
        "x86_64-unknown-linux-gnu", 24};
    const On1xSDK_TargetFeature required{"avx2", 4};
    const On1xSDK_TargetFeature forbidden{"neon", 4};
    On1xSDK_BackendCapability exact{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 1, 1, ON1X_SDK_ARTIFACT_MIR_TEXT,
        ON1X_SDK_BACKEND_DETERMINISTIC, &target_constraint, 1,
        &required, 1, &forbidden, 1};
    State exact_state{Mode::Good, "exact"};
    CHECK(add_provider(exact_registry.get(), "provider.exact", 0,
                       exact_state, exact) == ON1X_SDK_OK);
    auto exact_request = request(program.get());
    exact_request.target = avx.get();
    CHECK(on1x_sdk_backend_produce(exact_registry.get(), &exact_request, &raw) ==
          ON1X_SDK_OK);
    Artifact exact_artifact(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(exact_artifact.get()) == "exact");
    exact_request.target = plain.get();
    CHECK(on1x_sdk_backend_produce(exact_registry.get(), &exact_request, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    exact_request.target = nullptr;
    CHECK(on1x_sdk_backend_produce(exact_registry.get(), &exact_request, &raw) ==
          ON1X_SDK_UNSUPPORTED);

    auto native = capability;
    native.artifact_kind = ON1X_SDK_ARTIFACT_ASSEMBLY;
    CHECK(add_provider(exact_registry.get(), "provider.native", 0,
                       exact_state, native) == ON1X_SDK_UNSUPPORTED);
    On1xSDK_BackendProvider too_many{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "provider.limit", 14, 0, 0, &capability,
        ON1X_SDK_MAX_BACKEND_CAPABILITIES + 1u, run, &exact_state};
    CHECK(on1x_sdk_backend_provider_register(exact_registry.get(), &too_many) ==
          ON1X_SDK_LIMIT);
    auto invalid_capability = capability;
    invalid_capability.artifact_kind = ON1X_SDK_ARTIFACT_UNKNOWN;
    const On1xSDK_BackendCapability staged_capabilities[]{
        capability, invalid_capability};
    const On1xSDK_BackendProvider rolled_back{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        sizeof(On1xSDK_BackendProvider), "provider.rollback", 17, 0, 0,
        staged_capabilities, 2, run, &exact_state};
    CHECK(on1x_sdk_backend_provider_register(exact_registry.get(),
                                              &rolled_back) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(add_provider(exact_registry.get(), "provider.rollback", 0,
                       exact_state, capability) == ON1X_SDK_OK);

    CHECK(on1x_sdk_backend_produce(nullptr, &invoke, &raw) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_backend_produce(owner.get(), &invoke, nullptr) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_artifact_info(nullptr, &info) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(info == nullptr);
    const uint8_t *invalid_bytes = reinterpret_cast<const uint8_t *>(1);
    size_t invalid_size = 1;
    CHECK(on1x_sdk_artifact_bytes(nullptr, &invalid_bytes, &invalid_size) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(invalid_bytes == nullptr && invalid_size == 0);

    auto compatibility = registry();
    const On1xSDK_Backend legacy{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend), "legacy.v2-status",
        ON1X_SDK_VERIFIED_MIR, legacy_v2_status, nullptr};
    CHECK(on1x_sdk_backend_register(compatibility.get(), &legacy) == ON1X_SDK_OK);
    CHECK(on1x_sdk_backend_run(compatibility.get(), "legacy.v2-status",
                               malformed.get(), ON1X_SDK_MAX_ARTIFACT_BYTES,
                               &raw) == ON1X_SDK_ERROR);
    CHECK(raw == nullptr);

    auto negotiated = registry();
    auto short_capability = independent_capability();
    short_capability.flags = ON1X_SDK_BACKEND_TARGET_INDEPENDENT;
    short_capability.struct_size =
        offsetof(On1xSDK_BackendCapability, targets);
    const On1xSDK_BackendProvider short_provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        offsetof(On1xSDK_BackendProvider, userdata),
        "provider.short", 14, 0, 0, &short_capability, 1,
        no_userdata_run, reinterpret_cast<void *>(1)};
    CHECK(on1x_sdk_backend_provider_register(negotiated.get(),
                                              &short_provider) == ON1X_SDK_OK);
    auto short_request = request(program.get());
    short_request.provider_name = "provider.short";
    short_request.provider_name_size = 14;
    short_request.flags = ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY;
    short_request.poll = null_poll;
    short_request.poll_userdata = reinterpret_cast<void *>(1);
    short_request.struct_size =
        offsetof(On1xSDK_BackendRequest, poll_userdata);
    CHECK(on1x_sdk_backend_produce(negotiated.get(), &short_request, &raw) ==
          ON1X_SDK_OK);
    Artifact short_artifact(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(short_artifact.get()) == "short");
    CHECK(null_poll_calls == 6); /* pre, provider poll, post; repeated. */
    CHECK(on1x_sdk_artifact_info(short_artifact.get(), &info) == ON1X_SDK_OK);
    CHECK((info->flags & ON1X_SDK_ARTIFACT_DECLARED_DETERMINISTIC) == 0);
    CHECK(info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED);
    short_request.flags = ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC;
    CHECK(on1x_sdk_backend_produce(negotiated.get(), &short_request, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(raw == nullptr);

    On1xSDK_BackendInvocation short_invocation{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        offsetof(On1xSDK_BackendInvocation, poll_userdata),
        program.get(), nullptr, ON1X_SDK_ARTIFACT_MIR_TEXT, 1, 0,
        ON1X_SDK_MAX_ARTIFACT_BYTES, null_poll,
        reinterpret_cast<void *>(1)};
    CHECK(on1x_sdk_backend_poll(&short_invocation) == ON1X_SDK_OK);
    CHECK(null_poll_calls == 7);

    /* Staged bytes and metadata own all views independently of inputs/registry. */
    owner.reset();
    program.reset();
    CHECK(bytes(staged.get()) == "abc");
    CHECK(on1x_sdk_artifact_info(staged.get(), &info) == ON1X_SDK_OK);
    CHECK(std::string(info->provider_name, info->provider_name_size) ==
          "provider.high");
}
