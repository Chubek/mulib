# Include inside ON1X_WITH_CEPHYR after the baseline config and on1xc exist.
set(_kernel_rt "${CMAKE_CURRENT_LIST_DIR}/..")
set(_kernel_rt_clamp "${_kernel_rt}/../../include/mulib-clamp.h")
set(_kernel_rt_review "${CMAKE_CURRENT_SOURCE_DIR}/hir/FFIHandler/reviewed.json")
set(_kernel_rt_config "${cephyr_config_dir}/cephyr-baseline.toml")
set(_kernel_rt_embedded "${CMAKE_CURRENT_BINARY_DIR}/on1x_kernel_rt_embedded.inc")
add_custom_command(OUTPUT "${_kernel_rt_embedded}"
    COMMAND ${CMAKE_COMMAND} "-Dclamp=${_kernel_rt_clamp}" "-Dreview=${_kernel_rt_review}"
        "-Dconfig=${_kernel_rt_config}" "-Doutput=${_kernel_rt_embedded}"
        -P "${CMAKE_CURRENT_LIST_DIR}/EmbedCephyrKernels.cmake"
    DEPENDS "${_kernel_rt_clamp}" "${_kernel_rt_review}" "${_kernel_rt_config}"
        "${CMAKE_CURRENT_LIST_DIR}/EmbedCephyrKernels.cmake"
    VERBATIM)
find_package(Threads REQUIRED)
add_library(on1x_kernel_rt SHARED "${_kernel_rt}/src/On1xKernelRT.c" "${_kernel_rt_embedded}")
add_library(On1x::KernelRT ALIAS on1x_kernel_rt)
set_target_properties(on1x_kernel_rt PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
    C_VISIBILITY_PRESET default
    VERSION 1.0.0 SOVERSION 1
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$<TARGET_FILE_DIR:Cephyr::Cephyr>")
target_include_directories(on1x_kernel_rt
    PUBLIC "${_kernel_rt}/include" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(on1x_kernel_rt PRIVATE Cephyr::Cephyr Threads::Threads)
target_link_options(on1x_kernel_rt PRIVATE "LINKER:-z,defs")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_kernel_rt PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_dependencies(on1xc on1x_kernel_rt)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen.cpp" APPEND
    PROPERTY COMPILE_DEFINITIONS ON1X_HAS_CEPHYR_KERNEL_RT=1)
install(TARGETS on1x_kernel_rt LIBRARY DESTINATION lib)
install(FILES "${_kernel_rt}/include/On1xKernelRT.h"
    DESTINATION include)
install(FILES "${_kernel_rt}/include/On1xKernelRT.h"
    DESTINATION share/on1x/runtime-sdk/include)

if(BUILD_TESTING)
    add_executable(on1x_kernel_rt_test "${_kernel_rt}/tests/cephyr_kernels.c")
    set_target_properties(on1x_kernel_rt_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_kernel_rt_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_link_libraries(on1x_kernel_rt_test PRIVATE On1x::KernelRT)
    add_test(NAME on1x.runtime.cephyr_kernels COMMAND on1x_kernel_rt_test)
    set_tests_properties(on1x.runtime.cephyr_kernels PROPERTIES
        TIMEOUT 60 ENVIRONMENT "PATH=/nonexistent;CEPHYR_CONFIG=/nonexistent")
    add_test(NAME on1x.runtime.cephyr_stdffi_program
        COMMAND "${Python3_EXECUTABLE}" -B "${_kernel_rt}/tests/cephyr_stdffi_program.py"
            "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(on1x.runtime.cephyr_stdffi_program PROPERTIES TIMEOUT 90)
endif()
