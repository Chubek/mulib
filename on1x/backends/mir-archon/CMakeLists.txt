# Opt-in subdirectory. Include from on1x/backends after an installed,
# target-pinned ArchonBackend package is available.
cmake_minimum_required(VERSION 3.20)
find_package(ArchonBackend CONFIG REQUIRED)
if(NOT TARGET on1x_frontend)
  message(FATAL_ERROR "mir-archon must be added by On1x after on1x_frontend")
endif()

add_library(on1x_archon_bridge STATIC Bridge.cpp AsyncPlan.cpp)
target_compile_features(on1x_archon_bridge PUBLIC cxx_std_20)
target_include_directories(on1x_archon_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(on1x_archon_bridge PUBLIC on1x_frontend Archon::Backend)

# Link on1x_archon_scalar_abi into the generated executable (not the compiler).
# Its symbol names provide the exact scalar/float bit-pattern ABI contract.
add_library(on1x_archon_scalar_abi STATIC ScalarABI.c)
target_compile_features(on1x_archon_scalar_abi PRIVATE c_std_11)
target_link_libraries(on1x_archon_scalar_abi PUBLIC on1x_runtime)

add_executable(on1x-archon-mir Tool.cpp)
target_link_libraries(on1x-archon-mir PRIVATE on1x_archon_bridge)
install(TARGETS on1x-archon-mir RUNTIME DESTINATION bin)
install(TARGETS on1x_archon_scalar_abi ARCHIVE DESTINATION lib)

option(ON1X_ARCHON_ENABLE_SDK_ADAPTER "Build target-aware trusted SDK 2 provider" OFF)
set(ON1X_ARCHON_TARGET_TRIPLE "" CACHE STRING
    "Exact On1x SDK target triple corresponding to generated Archon::Backend")
if(NOT ON1X_ARCHON_TARGET_TRIPLE)
  message(FATAL_ERROR "Set ON1X_ARCHON_TARGET_TRIPLE for the generated native target")
endif()
target_compile_definitions(on1x-archon-mir PRIVATE
  ON1X_ARCHON_TARGET_TRIPLE="${ON1X_ARCHON_TARGET_TRIPLE}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
    AND ON1X_ARCHON_TARGET_TRIPLE STREQUAL "x86_64-unknown-linux-gnu")
  include(CheckCSourceCompiles)
  check_c_source_compiles("#include <features.h>
#if !defined(__GLIBC__) || !defined(__x86_64__) || !defined(__LP64__)
#error not the On1x CSP target ABI
#endif
int main(void) { return 0; }" ON1X_ARCHON_HAS_TIMER_ABI)
  if(ON1X_ARCHON_HAS_TIMER_ABI)
    find_package(Threads REQUIRED)
    add_library(on1x_archon_timer_abi STATIC TimerABI.c TimerKernels.c
      "${PROJECT_SOURCE_DIR}/runtime/on1x_concurrency.c")
    target_compile_features(on1x_archon_timer_abi PRIVATE c_std_11)
    target_include_directories(on1x_archon_timer_abi PRIVATE "${PROJECT_SOURCE_DIR}/../kernels")
    target_link_libraries(on1x_archon_timer_abi PUBLIC on1x_runtime Threads::Threads)
    target_compile_definitions(on1x_archon_bridge PRIVATE ON1X_ARCHON_TIMER_TARGET=1)
    install(TARGETS on1x_archon_timer_abi ARCHIVE DESTINATION lib)
  endif()
endif()
if(ON1X_ARCHON_ENABLE_SDK_ADAPTER)
  if(NOT TARGET On1x::SDK)
    message(FATAL_ERROR "mir-archon SDK adapter requires On1x::SDK")
  endif()
  add_library(on1x_backend_mir_archon STATIC Backend.cpp)
  target_include_directories(on1x_backend_mir_archon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/src")
  target_compile_features(on1x_backend_mir_archon PRIVATE cxx_std_20)
  target_compile_definitions(on1x_backend_mir_archon PRIVATE
    ON1X_ARCHON_TARGET_TRIPLE="${ON1X_ARCHON_TARGET_TRIPLE}")
  target_link_libraries(on1x_backend_mir_archon PRIVATE
    on1x_archon_bridge On1x::SDK)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME on1x.backends.mir_archon_tool_target
    COMMAND on1x-archon-mir --print-target)
  set_tests_properties(on1x.backends.mir_archon_tool_target PROPERTIES
    PASS_REGULAR_EXPRESSION "${ON1X_ARCHON_TARGET_TRIPLE}")
  add_executable(on1x_archon_test tests/TestBridge.cpp)
  target_link_libraries(on1x_archon_test PRIVATE on1x_archon_bridge)
  add_test(NAME on1x.backends.mir_archon COMMAND on1x_archon_test)
  add_executable(on1x_archon_tuple_phi_test tests/TestTuplePhi.cpp)
  target_link_libraries(on1x_archon_tuple_phi_test PRIVATE on1x_archon_bridge)
  add_test(NAME on1x.backends.mir_archon_tuple_phi COMMAND on1x_archon_tuple_phi_test)
  if(TARGET on1x_archon_timer_abi)
    add_executable(on1x_archon_timer_hir_test tests/TestTimerHIR.cpp)
    target_link_libraries(on1x_archon_timer_hir_test PRIVATE on1x_archon_bridge)
    add_test(NAME on1x.backends.mir_archon_timer_hir COMMAND on1x_archon_timer_hir_test)
    if(Python3_EXECUTABLE)
      set(_archon_timer_flags)
      foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
        list(APPEND _archon_timer_flags "--cflag=${_flag}")
      endforeach()
      add_test(NAME on1x.backends.mir_archon_timer_native
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/timer_native.py"
          --tool "$<TARGET_FILE:on1x-archon-mir>" --on1xc "$<TARGET_FILE:on1xc>"
          --cc "${CMAKE_C_COMPILER}" --work "${CMAKE_CURRENT_BINARY_DIR}/timer-native"
          --source "${PROJECT_SOURCE_DIR}" --timer-abi "$<TARGET_FILE:on1x_archon_timer_abi>"
          --runtime "$<TARGET_FILE:on1x_runtime>" ${_archon_timer_flags})
      set_tests_properties(on1x.backends.mir_archon_timer_native PROPERTIES TIMEOUT 180)
    endif()
  endif()
  if(Python3_EXECUTABLE)
    set(_archon_directives_test_args)
    if(ON1X_ARCHON_ENABLE_SDK_ADAPTER)
      list(APPEND _archon_directives_test_args sdk)
    endif()
    add_test(NAME on1x.backends.mir_archon_scoped_directives
      COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/mir/rewrite/test_dispatch.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_BINARY_DIR}"
        "$<TARGET_FILE:on1x-archon-mir>" ${_archon_directives_test_args})
    add_test(NAME on1x.backends.mir_archon_scalar_abi_regen
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_scalar_abi.py" --check)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
        AND ON1X_ARCHON_TARGET_TRIPLE STREQUAL "x86_64-unknown-linux-gnu")
      add_test(NAME on1x.backends.mir_archon_scalar_abi
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scalar_abi.py"
          --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
          --build-dir "${CMAKE_CURRENT_BINARY_DIR}/scalar-abi-tests"
          --native-tool "$<TARGET_FILE:on1x-archon-mir>")
      set_tests_properties(on1x.backends.mir_archon_scalar_abi PROPERTIES TIMEOUT 240)
      add_executable(on1x_archon_native_fixture tests/NativeFixture.cpp)
      target_compile_features(on1x_archon_native_fixture PRIVATE cxx_std_20)
      target_compile_options(on1x_archon_native_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
      target_link_libraries(on1x_archon_native_fixture PRIVATE on1x_archon_bridge)
      set(_archon_sdk_test_args)
      if(ON1X_ARCHON_ENABLE_SDK_ADAPTER)
        list(APPEND _archon_sdk_test_args --sdk-target "${ON1X_ARCHON_TARGET_TRIPLE}")
      endif()
      add_test(NAME on1x.backends.mir_archon_native_differential
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_differential.py"
          --fixture-tool "$<TARGET_FILE:on1x_archon_native_fixture>"
          --archon-tool "$<TARGET_FILE:on1x-archon-mir>"
          --on1xc "$<TARGET_FILE:on1xc>" --cc "${CMAKE_C_COMPILER}"
          --runtime "$<TARGET_FILE:on1x_runtime>"
          --scalar-abi "$<TARGET_FILE:on1x_archon_scalar_abi>"
          --runtime-include "${PROJECT_SOURCE_DIR}/runtime"
          --work "${CMAKE_CURRENT_BINARY_DIR}/native-differential" ${_archon_sdk_test_args})
      set_tests_properties(on1x.backends.mir_archon_native_differential PROPERTIES TIMEOUT 180)
      add_test(NAME on1x.backends.mir_archon_native_limits
        COMMAND on1x_archon_native_fixture --checks)
    endif()
  endif()
endif()
