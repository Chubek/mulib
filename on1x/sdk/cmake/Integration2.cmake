# This fragment runs after the canonical scanner/frontend targets exist.
target_link_libraries(on1x_sdk PRIVATE on1x_frontend)
target_include_directories(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
if(ON1X_GENERATED_FRONTEND)
    target_compile_definitions(on1x_sdk PRIVATE ON1X_SDK_GENERATED_SCANNER=1)
endif()

# Static SDK consumers need the canonical implementation archives. They are
# implementation dependencies, not additional public C++ header surfaces.
set_property(TARGET on1x_frontend PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
set_target_properties(on1x_frontend PROPERTIES EXPORT_NAME CompilerSupport)
set_target_properties(on1x_scanner PROPERTIES EXPORT_NAME ScannerSupport)
if(NOT TARGET On1x::CompilerSupport)
    add_library(On1x::CompilerSupport ALIAS on1x_frontend)
endif()
install(TARGETS on1x_frontend on1x_scanner
    EXPORT On1xSDKTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT On1xSDK
)
if(ON1X_GENERATED_FRONTEND)
    set_target_properties(aurocks_fault_runtime PROPERTIES EXPORT_NAME AurocksFaultRuntime)
    set_property(TARGET aurocks_fault_runtime PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${ON1X_LIBSTONE_ROOT}/cornerstone/Aurocks/include>")
    install(TARGETS aurocks_fault_runtime EXPORT On1xSDKTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT On1xSDK)
endif()
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../schemas/frontend"
    DESTINATION "share/on1x-sdk/schemas"
    COMPONENT On1xSDK
)

if(BUILD_TESTING)
    add_executable(on1x_sdk_test_frontend
        "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/frontend.cpp")
    target_link_libraries(on1x_sdk_test_frontend PRIVATE On1x::SDK)
    add_test(NAME on1x.sdk.frontend COMMAND on1x_sdk_test_frontend
        "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/fixtures")
    set_tests_properties(on1x.sdk.frontend PROPERTIES TIMEOUT 60)

    add_executable(on1x_sdk_test_frontend_c
        "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/frontend_c.c")
    target_link_libraries(on1x_sdk_test_frontend_c PRIVATE On1x::SDK)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_frontend_c PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.frontend_c COMMAND on1x_sdk_test_frontend_c)

    add_executable(on1x_sdk_test_scanner_parity
        "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/scanner_parity.cpp")
    target_include_directories(on1x_sdk_test_scanner_parity PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(on1x_sdk_test_scanner_parity
        PRIVATE On1x::SDK on1x_frontend)
    add_test(NAME on1x.sdk.frontend_scanner_parity
        COMMAND on1x_sdk_test_scanner_parity)

    # These tests use assert for both API calls and verification. Keep their
    # assertions active in Release without changing the library's build mode.
    foreach(_frontend_test IN ITEMS on1x_sdk_test_frontend
            on1x_sdk_test_frontend_c on1x_sdk_test_scanner_parity)
        if(MSVC)
            target_compile_options(${_frontend_test} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${_frontend_test} PRIVATE -UNDEBUG)
        endif()
    endforeach()

    add_executable(on1x_sdk_frontend_embed_example
        "${CMAKE_CURRENT_LIST_DIR}/../examples/frontend/embed.c")
    target_link_libraries(on1x_sdk_frontend_embed_example PRIVATE On1x::SDK)
    add_test(NAME on1x.sdk.frontend_example
        COMMAND on1x_sdk_frontend_embed_example)

    add_test(NAME on1x.sdk.frontend_headers
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/header_compile.py"
            "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_LIST_DIR}/../include"
            "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(on1x.sdk.frontend_headers PROPERTIES TIMEOUT 120)

    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME on1x.sdk.frontend_installed_consumer
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_LIST_DIR}/../tests/frontend/check_package.py"
                "${CMAKE_COMMAND}" "${CMAKE_CURRENT_BINARY_DIR}" "$<CONFIG>"
                "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
                "${CMAKE_CURRENT_LIST_DIR}/../examples/frontend/embed.c")
        set_tests_properties(on1x.sdk.frontend_installed_consumer
            PROPERTIES TIMEOUT 180)
    endif()
endif()
