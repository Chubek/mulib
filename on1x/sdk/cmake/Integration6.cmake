# Integration2 supplies the canonical frontend archive to On1x::SDK. Never
# compile, copy, or generate a separate SDK descriptor table.
set(_on1x_ffi_sdk "${CMAKE_CURRENT_LIST_DIR}/..")
target_sources(on1x_sdk PRIVATE "${_on1x_ffi_sdk}/src/contracts/FFIDescriptor.cpp")
if(TARGET on1x_ffi_tables AND
        EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/on1x/ffi.hpp")
    target_compile_definitions(on1x_sdk PRIVATE ON1X_SDK_HAS_TYPED_FFI=1)
    set(_on1x_ffi_sdk_typed ON)
endif()

if(BUILD_TESTING)
    add_executable(on1x_sdk_ffi_descriptor_c
        "${_on1x_ffi_sdk}/tests/contracts/ffi_descriptor.c")
    target_link_libraries(on1x_sdk_ffi_descriptor_c PRIVATE On1x::SDK)
    set_target_properties(on1x_sdk_ffi_descriptor_c PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(NOT _on1x_ffi_sdk_typed)
        add_test(NAME on1x.sdk6.ffi_descriptor_not_built
            COMMAND on1x_sdk_ffi_descriptor_c --unavailable)
    elseif(ON1X_FFI_TARGET STREQUAL "x86_64-linux-gnu" AND
           EXISTS "${ON1X_FFI_CATALOG}/clamp.ffi")
        add_executable(on1x_sdk_ffi_descriptor_cpp
            "${_on1x_ffi_sdk}/tests/contracts/ffi_descriptor.cpp")
        target_link_libraries(on1x_sdk_ffi_descriptor_cpp
            PRIVATE On1x::SDK on1x_frontend)
        target_compile_definitions(on1x_sdk_ffi_descriptor_cpp
            PRIVATE ON1X_SDK_FFI_TEST_CANONICAL=1)
        add_test(NAME on1x.sdk6.ffi_descriptor_c COMMAND on1x_sdk_ffi_descriptor_c)
        add_test(NAME on1x.sdk6.ffi_descriptor_cpp COMMAND on1x_sdk_ffi_descriptor_cpp)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(NAME on1x.sdk6.ffi_descriptor_installed
                COMMAND "${Python3_EXECUTABLE}" -B
                    "${_on1x_ffi_sdk}/tests/contracts/check_descriptor_package.py"
                    "${CMAKE_COMMAND}" "${CMAKE_BINARY_DIR}" "$<CONFIG>"
                    "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
                    "${CMAKE_C_FLAGS}" "${CMAKE_CXX_FLAGS}"
                    "${CMAKE_EXE_LINKER_FLAGS}")
            set_tests_properties(on1x.sdk6.ffi_descriptor_installed
                PROPERTIES TIMEOUT 300)
        endif()
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../runtime-sdk/cmake/FFIEmbedding.cmake")
