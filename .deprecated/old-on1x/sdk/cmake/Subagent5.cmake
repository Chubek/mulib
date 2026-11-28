# SDK 2 backend, target, artifact, native-probe and toolchain-resolver services.
target_sources(on1x_sdk PRIVATE
    src/backend/Artifact.cpp
    src/backend/NativeUnsupported.cpp
    src/backend/Target.cpp
    src/toolchain/Resolver.cpp
)
target_include_directories(on1x_sdk PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")
