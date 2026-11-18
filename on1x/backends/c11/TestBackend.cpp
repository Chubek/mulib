#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <on1x/ast.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory>

extern "C" {

On1xSDK_Status on1x_builtin_c11_register(On1xSDK_Registry *, on1x::backends::Context *);

static On1xSDK_Status unused_backend(void *, const On1xSDK_Program *, On1xSDK_OutputSink *) {
    std::abort();
}

}

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

int main() {
    on1x::backends::Context context;
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);

    const On1xSDK_Backend public_descriptor{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend), "c11", ON1X_SDK_CHECKED_HIR,
        unused_backend, nullptr};
    CHECK(on1x_sdk_backend_register(registry.get(), &public_descriptor) == ON1X_SDK_UNSUPPORTED);
    CHECK(on1x_builtin_c11_register(registry.get(), &context) == ON1X_SDK_OK);

    constexpr std::string_view source = "fn main() {\n  loop { break; }\n  println(42);\n}\n";
    const std::string source_name = "c11-backend.on1x";
    const auto check_source = [&] {
        auto unit = on1x::parse(on1x::scan(std::string(source), source_name));
        on1x::check(unit);
        return unit;
    };
    const auto unit = check_source();
    const auto expected = on1x::emit_c(unit, source_name);
    CHECK(!expected.empty());

    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x_sdk_program_create_source(source.data(), source.size(), &raw_program) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        unchecked(raw_program, on1x_sdk_program_destroy);
    On1xSDK_Artifact *raw_artifact = nullptr;
    CHECK(on1x_sdk_backend_run(registry.get(), "c11", unchecked.get(), expected.size(),
                               &raw_artifact) == ON1X_SDK_UNSUPPORTED);
    CHECK(!raw_artifact);

    CHECK(on1x::sdk::make_checked_hir(unit, source_name, &raw_program) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        checked(raw_program, on1x_sdk_program_destroy);
    CHECK(on1x_sdk_backend_run(registry.get(), "c11", checked.get(), expected.size(),
                               &raw_artifact) == ON1X_SDK_UNSUPPORTED);
    CHECK(!raw_artifact);
    CHECK(on1x::sdk::run_builtin_backend(registry.get(), "c11", checked.get(),
                                         &raw_artifact) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(bytes && std::string_view(reinterpret_cast<const char *>(bytes), size) == expected);
    CHECK(!context.error);

    // Deliberately violate the private bridge's checked-HIR precondition to
    // exercise the canonical emitter's error path without weakening the public ABI.
    auto malformed_unit = check_source();
    malformed_unit.functions.at(0).body.at(0).body.at(0).control_target = 0;
    CHECK(on1x::sdk::make_checked_hir(malformed_unit, source_name, &raw_program) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        malformed(raw_program, on1x_sdk_program_destroy);
    CHECK(on1x::sdk::run_builtin_backend(registry.get(), "c11", malformed.get(),
                                         &raw_artifact) == ON1X_SDK_ERROR);
    CHECK(!raw_artifact && context.error);
    try {
        std::rethrow_exception(context.error);
    } catch (const on1x::Error &error) {
        CHECK(error.where.file == source_name && error.where.line == 2);
        CHECK(error.diagnostic_id == "ON1X-CONTROL");
        return 0;
    }
}
