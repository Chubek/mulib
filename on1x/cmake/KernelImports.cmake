# Standalone runtime for direct C11 application imports. Keep it independent of
# Cephyr (a build-host tool), and of the legacy KernelRT and On1xRT engines.
if(NOT UNIX)
  return()
endif()

add_library(on1x_kernel_import STATIC
  runtime/on1x_kernel_import.c
  runtime/on1x_kernel_package.c
  runtime/on1x_kernel_recipe.c
  runtime/on1x_kernel_eval.c
  runtime/on1x_kernel_resolve.c
  runtime/on1x_kernel_native.c
  src/extensions_hash.c)
target_include_directories(on1x_kernel_import PUBLIC runtime PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../kernels")
install(TARGETS on1x_kernel_import ARCHIVE DESTINATION lib)

set(_qbfd_source "${ON1X_LIBSTONE_ROOT}/yellowstone/qbfd")
set(_on1x_native_kernel_default OFF)
set(_on1x_kernel_link_flags "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" AND
   CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "${_qbfd_source}/BFDBinBridge.cpp")
  include(CheckCSourceCompiles)
  check_c_source_compiles([=[
#include <features.h>
#if !defined(__GLIBC__) || !defined(__x86_64__) || defined(__ILP32__)
#error native kernel provider requires glibc with the AMD64 LP64 ABI
#endif
int main(void) { return 0; }
]=] ON1X_KERNEL_NATIVE_HAS_GLIBC)
  if(ON1X_KERNEL_NATIVE_HAS_GLIBC)
    set(_on1x_native_kernel_default ON)
  endif()
endif()
option(ON1X_WITH_KERNEL_NATIVE "Enable the operator-approved Linux AMD64 kernel provider"
  ${_on1x_native_kernel_default})
if(ON1X_WITH_KERNEL_NATIVE)
  if(NOT _on1x_native_kernel_default)
    message(FATAL_ERROR "ON1X_WITH_KERNEL_NATIVE requires glibc Linux AMD64, LP64, and LibStone qBFD")
  endif()
  if(NOT TARGET qbfd::qbfd)
    function(on1x_kernel_import_qbfd)
      # Pull in the private interface target without registering upstream
      # tests whose executables are excluded from this build.
      set(QBFD_BUILD_TESTS OFF)
      set(ARCHBFD_BUILD_TESTS OFF)
      set(ARCHBFD_BUILD_EXAMPLES OFF)
      add_subdirectory("${_qbfd_source}" "${CMAKE_CURRENT_BINARY_DIR}/kernel-import-qbfd" EXCLUDE_FROM_ALL)
    endfunction()
    on1x_kernel_import_qbfd()
  endif()
  # Include the private, compiled C API bridge in the same archive so that
  # generated applications do not need a separate ArchBFD deployment.
  target_sources(on1x_kernel_import PRIVATE
    "${_qbfd_source}/BFDBinABI.c" "${_qbfd_source}/BFDBinBridge.cpp")
  target_link_libraries(on1x_kernel_import PRIVATE qbfd::qbfd ${CMAKE_DL_LIBS})
  target_compile_definitions(on1x_kernel_import PRIVATE ON1X_KERNEL_NATIVE_QBFD=1)
  if("c++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
    string(APPEND _on1x_kernel_link_flags "-lc++\n")
  else()
    string(APPEND _on1x_kernel_link_flags "-lstdc++\n")
  endif()
  if("dl" IN_LIST CMAKE_DL_LIBS)
    string(APPEND _on1x_kernel_link_flags "-ldl\n")
  endif()
endif()

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/on1x_kernel_import.link"
  CONTENT "${_on1x_kernel_link_flags}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/on1x_kernel_import.link" DESTINATION lib)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_executable(on1x_kernel_eval_tests tests/test_kernel_eval.c)
  target_link_libraries(on1x_kernel_eval_tests PRIVATE on1x_kernel_import)
  add_test(NAME on1x.kernel_load_eval COMMAND on1x_kernel_eval_tests)
  add_executable(on1x_kernel_resolve_tests tests/test_kernel_resolve.c)
  target_link_libraries(on1x_kernel_resolve_tests PRIVATE on1x_kernel_import)
  add_test(NAME on1x.kernel_load_resolve COMMAND on1x_kernel_resolve_tests)
  add_executable(on1x_kernel_recipe_tests tests/test_kernel_recipe.c)
  target_link_libraries(on1x_kernel_recipe_tests PRIVATE on1x_kernel_import)
  add_test(NAME on1x.kernel_load_recipe COMMAND on1x_kernel_recipe_tests)
  add_test(NAME on1x.kernel_load_legacy COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_load_legacy.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts"
    "${CMAKE_C_COMPILER}" ${ON1X_TEST_C_FLAGS})
  set_tests_properties(on1x.kernel_load_legacy PROPERTIES TIMEOUT 180)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_kernel_negative_cflags "")
    foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
      list(APPEND _kernel_negative_cflags "--cflag=${_flag}")
    endforeach()
    if(ON1X_WITH_KERNEL_NATIVE)
      list(APPEND _kernel_negative_cflags --native)
    endif()
    add_test(NAME on1x.kernel_load_negative COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_load_negative.py" "$<TARGET_FILE:on1xc>"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-load-negative"
      "${CMAKE_C_COMPILER}" ${_kernel_negative_cflags} --import-library "$<TARGET_FILE:on1x_kernel_import>")
    set_tests_properties(on1x.kernel_load_negative PROPERTIES TIMEOUT 180)
  endif()
endif()
