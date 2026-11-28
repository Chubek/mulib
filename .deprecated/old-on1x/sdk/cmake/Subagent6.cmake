# Subagent 6: structured foreign admission and the independent On1xRT ABI.

target_sources(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/contracts/ForeignAdmission.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/contracts/CddAccessors.c"
)
target_compile_features(on1x_sdk PRIVATE c_std_11)
target_include_directories(on1x_sdk PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../contracts-exotract/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../kernels>"
)
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts-exotract/include/mulib-cdd-accessors.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../kernels/mulib-cdd-catalogue.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../kernels/mulib-cdd-plan.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/on1x-sdk"
    COMPONENT On1xSDK
)

option(ON1X_RT_BUILD_SHARED "Build On1xRT as a shared library for native and Python hosts" OFF)
set(_on1x_rt_linkage STATIC)
if(ON1X_RT_BUILD_SHARED)
    set(_on1x_rt_linkage SHARED)
endif()
add_library(on1x_rt_sdk ${_on1x_rt_linkage}
    "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/src/On1xRT.c"
)
add_library(On1x::RT ALIAS on1x_rt_sdk)
add_library(On1x::Runtime ALIAS on1x_rt_sdk)
set_target_properties(on1x_rt_sdk PROPERTIES
    EXPORT_NAME RT
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    BUILD_WITH_INSTALL_RPATH ON
    SOVERSION 1
)
target_compile_features(on1x_rt_sdk PUBLIC c_std_11)
target_include_directories(on1x_rt_sdk PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/on1x-runtime>"
)
find_package(Threads REQUIRED)
target_link_libraries(on1x_rt_sdk PUBLIC Threads::Threads)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_sdk PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Wpedantic;-Werror>
    )
    target_compile_options(on1x_rt_sdk PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

set(_on1x_rt_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/On1xRT")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/On1xRTConfig.cmake"
    "include(CMakeFindDependencyMacro)\n"
    "find_dependency(Threads)\n"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/On1xRTTargets.cmake\")\n")
export(
    TARGETS on1x_rt_sdk
    NAMESPACE On1x::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/On1xRTTargets.cmake"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/On1xRTConfigVersion.cmake"
    VERSION 1.1.0
    COMPATIBILITY SameMajorVersion
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/on1x-runtime"
    COMPONENT On1xRT
)
install(
    TARGETS on1x_rt_sdk
    EXPORT On1xRTTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT On1xRT
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT On1xRT
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT On1xRT
)
install(
    EXPORT On1xRTTargets
    NAMESPACE On1x::
    DESTINATION "${_on1x_rt_package_dir}"
    COMPONENT On1xRT
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/On1xRTConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/On1xRTConfigVersion.cmake"
    DESTINATION "${_on1x_rt_package_dir}"
    COMPONENT On1xRT
)

if(BUILD_TESTING)
    add_executable(on1x_sdk6_test_contracts
        tests/contracts/foreign_admission.c
    )
    target_link_libraries(on1x_sdk6_test_contracts PRIVATE On1x::SDK)
    add_test(NAME on1x.sdk6.foreign_admission
        COMMAND on1x_sdk6_test_contracts)

    add_executable(on1x_sdk6_test_cdd
        tests/contracts/cdd_accessors.c
    )
    target_link_libraries(on1x_sdk6_test_cdd PRIVATE On1x::SDK)
    target_include_directories(on1x_sdk6_test_cdd PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts-exotract/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../kernels"
    )
    add_test(NAME on1x.sdk6.cdd_accessors
        COMMAND on1x_sdk6_test_cdd)

    add_executable(on1x_sdk6_test_cpp
        tests/contracts/foreign_cpp.cpp
    )
    target_link_libraries(on1x_sdk6_test_cpp PRIVATE On1x::SDK)
    add_test(NAME on1x.sdk6.foreign_cpp COMMAND on1x_sdk6_test_cpp)

    add_library(on1x_rt_test_module STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/tests/fixture_module.c"
    )
    target_link_libraries(on1x_rt_test_module PUBLIC On1x::RT)

    foreach(_on1x_rt_test IN ITEMS lifecycle negative fault)
        add_executable(on1x_rt_test_${_on1x_rt_test}
            "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/tests/${_on1x_rt_test}.c"
        )
        target_link_libraries(on1x_rt_test_${_on1x_rt_test}
            PRIVATE on1x_rt_test_module)
        add_test(NAME on1x.sdk6.runtime_${_on1x_rt_test}
            COMMAND on1x_rt_test_${_on1x_rt_test})
    endforeach()

    add_executable(on1x_rt_test_cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/tests/host.cpp"
    )
    target_link_libraries(on1x_rt_test_cpp PRIVATE on1x_rt_test_module)
    add_test(NAME on1x.sdk6.runtime_cpp COMMAND on1x_rt_test_cpp)

    add_executable(on1x_rt_test_abi
        "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-sdk/tests/abi.c"
    )
    target_link_libraries(on1x_rt_test_abi PRIVATE On1x::RT)
    add_test(NAME on1x.sdk6.runtime_abi COMMAND on1x_rt_test_abi)

    foreach(_on1x_sdk6_target IN ITEMS
            on1x_sdk6_test_contracts on1x_sdk6_test_cdd
            on1x_sdk6_test_cpp
            on1x_rt_test_module on1x_rt_test_lifecycle
            on1x_rt_test_negative on1x_rt_test_fault
            on1x_rt_test_cpp on1x_rt_test_abi)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${_on1x_sdk6_target} PRIVATE
                $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Wpedantic;-Werror>
                $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Werror>
            )
        endif()
    endforeach()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../runtime-sdk/cmake/Embedding.cmake")
