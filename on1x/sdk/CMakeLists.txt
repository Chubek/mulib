include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(on1x_sdk STATIC
    src/Analyzer.cpp
    src/Backend.cpp
    src/CLIOpt.cpp
    src/Contract.cpp
    src/ExecEnv.cpp
    src/Module.cpp
    src/Registry.cpp
    src/Toolchain.cpp
    src/Transformer.cpp
)
add_library(On1x::SDK ALIAS on1x_sdk)
set_target_properties(on1x_sdk PROPERTIES EXPORT_NAME SDK POSITION_INDEPENDENT_CODE ON)

target_compile_features(on1x_sdk PRIVATE cxx_std_20)

target_include_directories(
    on1x_sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/on1x-sdk>
)

set(_sdk_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/On1xSDK")
set(ON1X_SDK_PACKAGE_VERSION 2.0.0)
configure_package_config_file(cmake/On1xSDKConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/On1xSDKConfig.cmake"
    INSTALL_DESTINATION "${_sdk_package_dir}"
)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/On1xSDKVersion2.cmake"
    VERSION "${ON1X_SDK_PACKAGE_VERSION}" COMPATIBILITY SameMajorVersion
)
configure_file(cmake/On1xSDKConfigVersion.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/On1xSDKConfigVersion.cmake" COPYONLY)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/on1x-sdk"
    COMPONENT On1xSDK
)
install(
    FILES
        ARCHITECTURE.md
        ABI.md
        COMPATIBILITY.md
        EXTENSIONS.md
        IMPLEMENTATION.md
        README.md
        SECURITY.md
        VERIFICATION.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/sdk"
    COMPONENT On1xSDK
)
install(
    FILES CAPABILITY_MATRIX.json IMPLEMENTATION_INVENTORY.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/on1x-sdk"
    COMPONENT On1xSDK
)
install(
    DIRECTORY protocol schemas catalogue
    DESTINATION "${CMAKE_INSTALL_DATADIR}/on1x-sdk"
    COMPONENT On1xSDK
)
install(
    DIRECTORY docs/
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/sdk"
    COMPONENT On1xSDK
)

install(
    TARGETS on1x_sdk
    EXPORT On1xSDKTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT On1xSDK
)

install(
    EXPORT On1xSDKTargets
    NAMESPACE On1x::
    DESTINATION "${_sdk_package_dir}"
    COMPONENT On1xSDK
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/On1xSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/On1xSDKConfigVersion.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/On1xSDKVersion2.cmake"
    DESTINATION "${_sdk_package_dir}" COMPONENT On1xSDK
)

if(BUILD_TESTING)
    foreach(_facet IN ITEMS registry analyzer backend security)
        add_executable(on1x_sdk_test_${_facet} tests/${_facet}.cpp)
        target_link_libraries(on1x_sdk_test_${_facet} PRIVATE On1x::SDK)
        target_include_directories(on1x_sdk_test_${_facet} PRIVATE src)
        add_test(NAME on1x.sdk.${_facet} COMMAND on1x_sdk_test_${_facet})
    endforeach()
    add_executable(on1x_sdk_test_c_abi tests/c_abi.c)
    target_link_libraries(on1x_sdk_test_c_abi PRIVATE On1x::SDK)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_c_abi PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.c_abi COMMAND on1x_sdk_test_c_abi)
endif()

# SDK 2 workers own disjoint optional fragments. Keeping these hooks here avoids
# concurrent edits to this aggregate file; absent fragments are intentionally
# ignored during staged integration.
foreach(_on1x_sdk_subagent RANGE 1 8)
    include("cmake/Subagent${_on1x_sdk_subagent}.cmake" OPTIONAL)
endforeach()

if(TARGET on1x_sdk_semantics_ir)
    set_target_properties(on1x_sdk_semantics_ir PROPERTIES
        EXPORT_NAME SDKSemanticsIR)
    install(
        TARGETS on1x_sdk_semantics_ir
        EXPORT On1xSDKTargets
        COMPONENT On1xSDK
    )
endif()

if(BUILD_TESTING)
    add_executable(on1x_sdk_test_aggregate_c tests/aggregate_c.c)
    target_link_libraries(on1x_sdk_test_aggregate_c
        PRIVATE On1x::SDKSemanticsIR)
    set_target_properties(on1x_sdk_test_aggregate_c PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_aggregate_c PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.aggregate_c COMMAND on1x_sdk_test_aggregate_c)

    add_executable(on1x_sdk_test_aggregate_cpp tests/aggregate_cpp.cpp)
    target_link_libraries(on1x_sdk_test_aggregate_cpp
        PRIVATE On1x::Project)
    set_target_properties(on1x_sdk_test_aggregate_cpp PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_aggregate_cpp PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.aggregate_cpp COMMAND on1x_sdk_test_aggregate_cpp)
endif()
