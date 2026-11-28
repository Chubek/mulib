#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
On1xSDK_Status on1x_builtin_c11_register(On1xSDK_Registry *,
                                         on1x::backends::Context *);
On1xSDK_Status on1x_builtin_mir_c11_register(On1xSDK_Registry *,
                                             on1x::backends::Context *);
On1xSDK_Status on1x_builtin_mir_cfg_register(On1xSDK_Registry *,
                                             on1x::backends::Context *);
On1xSDK_Status on1x_builtin_mir_eval_register(On1xSDK_Registry *,
                                              on1x::backends::Context *);
On1xSDK_Status on1x_builtin_mir_text_register(On1xSDK_Registry *,
                                              on1x::backends::Context *);
}

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

std::string produce(const On1xSDK_Registry *registry,
                    const On1xSDK_Program *program, const char *provider,
                    On1xSDK_ArtifactKind kind) {
    const On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program, nullptr, kind, 1, provider, std::strlen(provider),
        ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY, 0,
        ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr};
    On1xSDK_Artifact *raw = nullptr;
    CHECK(on1x_sdk_backend_produce(registry, &request, &raw) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw, on1x_sdk_artifact_destroy);
    const On1xSDK_ArtifactInfo *info = nullptr;
    CHECK(on1x_sdk_artifact_info(artifact.get(), &info) == ON1X_SDK_OK);
    CHECK(info->kind == kind &&
          std::string(info->provider_name, info->provider_name_size) == provider &&
          (info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED));
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    return size ? std::string(reinterpret_cast<const char *>(bytes), size)
                : std::string{};
}

} // namespace

int main() {
    constexpr std::string_view wire =
        "(on1x.mir 1 1 ((function 2 \"main\" i64 (at \"main.on1x\" 0 1 1) () "
        "((block 3 4 (at \"main.on1x\" 0 1 1) () "
        "((instruction 5 const i64 () 42 0 0 0 (at \"main.on1x\" 26 1 27))) "
        "(exit 6 return 5 4 () (at \"main.on1x\" 19 1 20)))))))\n";
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    on1x::backends::Context c11, mir_c11, cfg, eval, text;
    CHECK(on1x_builtin_c11_register(registry.get(), &c11) == ON1X_SDK_OK);
    CHECK(on1x_builtin_mir_c11_register(registry.get(), &mir_c11) == ON1X_SDK_OK);
    CHECK(on1x_builtin_mir_cfg_register(registry.get(), &cfg) == ON1X_SDK_OK);
    CHECK(on1x_builtin_mir_eval_register(registry.get(), &eval) == ON1X_SDK_OK);
    CHECK(on1x_builtin_mir_text_register(registry.get(), &text) == ON1X_SDK_OK);

    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x::sdk::make_verified_mir(wire, &raw_program) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    CHECK(produce(registry.get(), program.get(), "mir-text",
                  ON1X_SDK_ARTIFACT_MIR_TEXT) == wire);
    CHECK(produce(registry.get(), program.get(), "mir-cfg",
                  ON1X_SDK_ARTIFACT_CFG_DOT).starts_with("digraph on1x_mir"));
    CHECK(produce(registry.get(), program.get(), "mir-c11",
                  ON1X_SDK_ARTIFACT_C11_SOURCE).find("#include") !=
          std::string::npos);
    CHECK(produce(registry.get(), program.get(), "mir-eval",
                  ON1X_SDK_ARTIFACT_PROGRAM_OUTPUT).empty());
    CHECK(eval.exit_code == 42);
}
