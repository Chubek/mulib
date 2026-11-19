#include "On1xSDK-Artifact.h"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"
#include "external_backend.h"

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

int main() {
    constexpr std::string_view mir =
        "(on1x.mir 1 1 ((function 2 \"main\" i64 (at \"main.on1x\" 0 1 1) () "
        "((block 3 4 (at \"main.on1x\" 0 1 1) () "
        "((instruction 5 const i64 () 42 0 0 0 (at \"main.on1x\" 26 1 27))) "
        "(exit 6 return 5 4 () (at \"main.on1x\" 19 1 20)))))))\n";
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    CHECK(on1x_sdk_example_backend_register(registry.get()) == ON1X_SDK_OK);
    CHECK(on1x_sdk_example_backend_register(registry.get()) == ON1X_SDK_DUPLICATE);

    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x::sdk::make_verified_mir(mir, &raw_program) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    const On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program.get(), nullptr, ON1X_SDK_ARTIFACT_MIR_TEXT, 1,
        "example.receipt", sizeof("example.receipt") - 1,
        ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY, 0,
        ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr};
    On1xSDK_Artifact *raw_artifact = nullptr;
    CHECK(on1x_sdk_backend_produce(registry.get(), &request, &raw_artifact) ==
          ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    constexpr std::string_view prefix = "verified-mir:";
    CHECK(size == prefix.size() + mir.size());
    CHECK(std::memcmp(bytes, prefix.data(), prefix.size()) == 0);
    CHECK(std::memcmp(bytes + prefix.size(), mir.data(), mir.size()) == 0);
    const On1xSDK_ArtifactInfo *info = nullptr;
    CHECK(on1x_sdk_artifact_info(artifact.get(), &info) == ON1X_SDK_OK);
    CHECK(info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED);
}
