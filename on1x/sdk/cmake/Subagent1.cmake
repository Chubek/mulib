target_sources(on1x_sdk PRIVATE
    src/core/Core.cpp
    src/core/Diagnostic.cpp
    src/core/HandleStore.cpp
    src/core/Protocol.cpp
    src/core/SharedMemory.cpp
    src/core/Transport.cpp
    src/core/CoreHashKernels.c
    src/core/CoreIPCKernels.c
    src/catalogue/Catalogue.cpp
    src/catalogue/CatalogueKernels.c
    src/plugin/Manifest.cpp
    src/plugin/Plugin.cpp
)

target_include_directories(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
target_compile_features(on1x_sdk PRIVATE c_std_11 cxx_std_20)

if(BUILD_TESTING)
    foreach(_on1x_core_test IN ITEMS core_v2 registry_v2 catalogue_v2 plugin_v2)
        add_executable(on1x_sdk_test_${_on1x_core_test}
            "tests/${_on1x_core_test}.cpp")
        target_link_libraries(on1x_sdk_test_${_on1x_core_test}
            PRIVATE On1x::SDK)
        target_include_directories(on1x_sdk_test_${_on1x_core_test}
            PRIVATE src)
        add_test(NAME on1x.sdk.${_on1x_core_test}
            COMMAND on1x_sdk_test_${_on1x_core_test})
    endforeach()

    add_executable(on1x_sdk_test_abi2_c tests/abi2_c.c)
    target_link_libraries(on1x_sdk_test_abi2_c PRIVATE On1x::SDK)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_abi2_c PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.abi2_c COMMAND on1x_sdk_test_abi2_c)
endif()
