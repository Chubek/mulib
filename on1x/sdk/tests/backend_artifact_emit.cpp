#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string_view>

extern "C" On1xSDK_Status on1x_builtin_mir_c11_register(
    On1xSDK_Registry *, on1x::backends::Context *);

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    constexpr std::string_view mir =
        "(on1x.mir 1 1 ((function 2 \"main\" i64 (at \"main.on1x\" 0 1 1) () "
        "((block 3 4 (at \"main.on1x\" 0 1 1) () "
        "((instruction 5 const i64 () 42 0 0 0 (at \"main.on1x\" 26 1 27))) "
        "(exit 6 return 5 4 () (at \"main.on1x\" 19 1 20)))))))\n";
    On1xSDK_Registry *raw_registry = nullptr;
    if (on1x_sdk_registry_create(&raw_registry) != ON1X_SDK_OK) return 3;
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    on1x::backends::Context context;
    if (on1x_builtin_mir_c11_register(registry.get(), &context) != ON1X_SDK_OK)
        return 4;
    On1xSDK_Program *raw_program = nullptr;
    if (on1x::sdk::make_verified_mir(mir, &raw_program) != ON1X_SDK_OK) return 5;
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    const On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program.get(), nullptr, ON1X_SDK_ARTIFACT_C11_SOURCE, 1,
        "mir-c11", 7, ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY, 0,
        ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr};
    On1xSDK_Artifact *raw_artifact = nullptr;
    if (on1x_sdk_backend_produce(registry.get(), &request, &raw_artifact) !=
        ON1X_SDK_OK)
        return 6;
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const On1xSDK_ArtifactInfo *info = nullptr;
    if (on1x_sdk_artifact_info(artifact.get(), &info) != ON1X_SDK_OK ||
        !(info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED))
        return 7;
    const std::uint8_t *bytes = nullptr;
    std::size_t size = 0;
    if (on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) != ON1X_SDK_OK ||
        !bytes || !size)
        return 8;
    std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(bytes),
                 static_cast<std::streamsize>(size));
    return output ? 0 : 9;
}
