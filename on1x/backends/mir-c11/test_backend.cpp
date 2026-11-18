#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <on1x/backend/mir.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

extern "C" On1xSDK_Status on1x_builtin_mir_c11_register(On1xSDK_Registry *,
                                                       on1x::backends::Context *);

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

namespace {

using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;

Program wire_program(std::string_view wire) {
    On1xSDK_Program *program = nullptr;
    CHECK(on1x::sdk::make_verified_mir(wire, &program) == ON1X_SDK_OK);
    return {program, on1x_sdk_program_destroy};
}

std::string read_artifact(On1xSDK_Artifact *raw, On1xSDK_Status status, On1xSDK_Status expected) {
    CHECK(status == expected);
    if (expected != ON1X_SDK_OK) {
        CHECK(!raw);
        return {};
    }
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw, on1x_sdk_artifact_destroy);
    const std::uint8_t *bytes = nullptr;
    std::size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(bytes && size);
    return {reinterpret_cast<const char *>(bytes), size};
}

std::string run_public(const On1xSDK_Registry *registry, const Program &program, std::size_t limit,
                       On1xSDK_Status expected = ON1X_SDK_OK) {
    On1xSDK_Artifact *raw = nullptr;
    const auto status = on1x_sdk_backend_run(registry, "mir-c11", program.get(), limit, &raw);
    return read_artifact(raw, status, expected);
}

std::string run_builtin(const On1xSDK_Registry *registry, const on1x::mir::Module &module,
                        On1xSDK_Status expected = ON1X_SDK_OK) {
    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x::sdk::make_builtin_mir(module, &raw_program) == ON1X_SDK_OK);
    const Program program(raw_program, on1x_sdk_program_destroy);
    On1xSDK_Artifact *raw = nullptr;
    const auto status = on1x::sdk::run_builtin_backend(registry, "mir-c11", program.get(), &raw);
    return read_artifact(raw, status, expected);
}

void check_error(const on1x::backends::Context &context, std::string_view prefix,
                 const on1x::Location &location) {
    CHECK(context.error);
    try {
        std::rethrow_exception(context.error);
    } catch (const on1x::Error &error) {
        CHECK(std::string_view(error.what()).starts_with(prefix));
        CHECK(error.where.file == location.file && error.where.offset == location.offset &&
              error.where.line == location.line && error.where.column == location.column);
        return;
    }
    std::abort();
}

} // namespace

int main() {
    on1x::backends::Context context;
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    CHECK(on1x_builtin_mir_c11_register(registry.get(), nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_builtin_mir_c11_register(registry.get(), &context) == ON1X_SDK_OK);
    CHECK(on1x_builtin_mir_c11_register(registry.get(), &context) == ON1X_SDK_DUPLICATE);

    namespace mir = on1x::mir;
    // An imported location with escaped bytes, distinct from the MIR wire source.
    const on1x::Location imported{"imported/quote\"slash\\\xc3\xa9.on1x", 250, 17, 19};
    const on1x::Location where{"main.on1x", 123, 7, 13};
    mir::Module module;
    module.functions.push_back({2, "main", on1x::Type::I64, where, {}, {
        {3, 4, where, {}, {{5, on1x::Type::I64, mir::Op::Constant, {}, 42, 0, 0, 0, imported}},
         {6, mir::Exit::Return, 5, 4, {}, where}}
    }});
    auto wire = mir::serialize(module);
    const auto expected = mir::emit_c(module);
    CHECK(run_builtin(registry.get(), module) == expected);
    CHECK(run_public(registry.get(), wire_program(wire), expected.size()) == expected);
    run_public(registry.get(), wire_program(wire), expected.size() - 1, ON1X_SDK_LIMIT);
    CHECK(!context.error);

    On1xSDK_Program *raw_source = nullptr;
    CHECK(on1x_sdk_program_create_source(wire.data(), wire.size(), &raw_source) == ON1X_SDK_OK);
    const Program source(raw_source, on1x_sdk_program_destroy);
    run_public(registry.get(), source, expected.size(), ON1X_SDK_UNSUPPORTED);

    // Wrong opcode signature: corrupt only the wire, then check the verifier's
    // reported location rather than its position in the serialized S-expression.
    wire.replace(wire.find("const i64"), sizeof("const i64") - 1, "const void");
    run_public(registry.get(), wire_program(wire), expected.size(), ON1X_SDK_ERROR);
    check_error(context, "ON1X-MIR-VERIFY:", imported);
    run_public(registry.get(), wire_program({}), expected.size(), ON1X_SDK_ERROR);
    check_error(context, "S-expression:", {"<mir>"});

    auto no_main = module;
    no_main.functions.front().name = "helper";
    run_builtin(registry.get(), no_main, ON1X_SDK_ERROR);
    check_error(context, "ON1X-MIR-C11:", {"<mir>"});
    run_public(registry.get(), wire_program(mir::serialize(no_main)), expected.size(), ON1X_SDK_ERROR);
    check_error(context, "ON1X-MIR-C11:", {"<mir>"});
    CHECK(run_builtin(registry.get(), module) == expected);
    CHECK(!context.error);

    // Verified in memory, but \xHH-escaped filenames make its wire >4 MiB.
    auto &instructions = module.functions.front().blocks.front().instructions;
    auto repeated = instructions.front();
    repeated.where.file = std::string(1800, static_cast<char>(0xab));
    for (mir::Id id = 100; id < 800; ++id) {
        repeated.id = id;
        instructions.push_back(repeated);
    }
    const auto larger_c = mir::emit_c(module);
    try {
        (void)mir::serialize(module);
        CHECK(false);
    } catch (const on1x::Error &error) {
        CHECK(std::string_view(error.what()).find("output byte limit") != std::string_view::npos);
    }
    CHECK(run_builtin(registry.get(), module) == larger_c);
}
