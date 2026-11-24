if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" OR
   NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "ON1X_WITH_ARCHVIRT requires a native x86-64 Linux build")
endif()

set(ON1X_ARCHVIRT_ROOT "" CACHE PATH "Installation prefix of the pinned LibStone ArchVirt")
if(NOT IS_ABSOLUTE "${ON1X_ARCHVIRT_ROOT}")
  message(FATAL_ERROR "Set ON1X_ARCHVIRT_ROOT to an absolute prefix for the pinned LibStone ArchVirt installation")
endif()
unset(_archvirt_include CACHE)
unset(_archvirt_library CACHE)
find_path(_archvirt_include archvirt.h HINTS "${ON1X_ARCHVIRT_ROOT}/include"
  NO_DEFAULT_PATH)
find_library(_archvirt_library NAMES domvirt HINTS "${ON1X_ARCHVIRT_ROOT}/lib"
  "${ON1X_ARCHVIRT_ROOT}/lib64" "${ON1X_ARCHVIRT_ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
  NO_DEFAULT_PATH)
mark_as_advanced(_archvirt_include _archvirt_library)
if(NOT _archvirt_include OR NOT EXISTS "${_archvirt_include}/archvirt_boot.h" OR
   NOT _archvirt_library OR _archvirt_library MATCHES "\\.a$")
  message(FATAL_ERROR "ON1X_ARCHVIRT_ROOT must contain the pinned ArchVirt headers and shared libdomvirt")
endif()
add_library(On1xArchVirt::Library SHARED IMPORTED)
set_target_properties(On1xArchVirt::Library PROPERTIES
  IMPORTED_LOCATION "${_archvirt_library}"
  INTERFACE_INCLUDE_DIRECTORIES "${_archvirt_include}")
get_filename_component(_archvirt_libdir "${_archvirt_library}" DIRECTORY)
add_subdirectory(stdlib/vmm)

include(CMakePushCheckState)
include(CheckCSourceCompiles)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES On1xArchVirt::Library)
unset(ON1X_ARCHVIRT_C_API CACHE)
check_c_source_compiles([[#include <archvirt.h>
  int main(void) {
    ArchVirtConfig config = {0};
    config.struct_size = sizeof config;
    void *vm = archvirt_create();
    if (!vm) return 1;
    const int result = archvirt_set_serial(vm, 0, 0) ||
        archvirt_initialize(vm, &config) || archvirt_start(vm) || archvirt_wait(vm);
    archvirt_stop(vm);
    const char *error = archvirt_last_error(vm);
    archvirt_destroy(vm);
    return result || !error || !archvirt_version();
  }]] ON1X_ARCHVIRT_C_API)
cmake_pop_check_state()
if(NOT ON1X_ARCHVIRT_C_API)
  message(FATAL_ERROR "The selected libdomvirt does not export the pinned ArchVirt C API")
endif()

find_program(ON1X_ARCHVIRT_CLANG NAMES clang REQUIRED)
find_program(ON1X_ARCHVIRT_LLD NAMES ld.lld REQUIRED)
find_package(Threads REQUIRED)
set(_archvirt_source "${CMAKE_CURRENT_SOURCE_DIR}/archvirt")
set(_archvirt_runtime "${CMAKE_CURRENT_BINARY_DIR}/on1x-archvirt-runtime.a")
add_custom_command(OUTPUT "${_archvirt_runtime}"
  COMMAND "${Python3_EXECUTABLE}" -IB "${_archvirt_source}/compile.py"
    --clang "${ON1X_ARCHVIRT_CLANG}" --build-runtime "${CMAKE_AR}"
    --runtime-include "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    --archvirt-include "${_archvirt_include}" -o "${_archvirt_runtime}"
  DEPENDS archvirt/compile.py archvirt/guest.c archvirt/guest.ld archvirt/start.S
    archvirt/protocol.h archvirt/include/string.h archvirt/include/on1x_archvirt_guest.h runtime/on1x_runtime.h
    "${_archvirt_include}/archvirt_boot.h" "${ON1X_ARCHVIRT_CLANG}" "${CMAKE_AR}"
  VERBATIM)
add_custom_target(on1x_archvirt_runtime DEPENDS "${_archvirt_runtime}")

add_library(on1x_archvirt_host STATIC archvirt/runner.c archvirt/host_stage.c)
target_link_libraries(on1x_archvirt_host PUBLIC On1xArchVirt::Library Threads::Threads)
add_executable(on1x-archvirt-wrapper archvirt/linked_main.c)
target_link_libraries(on1x-archvirt-wrapper PRIVATE on1x_archvirt_host)
set_target_properties(on1x-archvirt-wrapper PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "${_archvirt_libdir}")
add_executable(on1x-archvirt-run archvirt/run_main.c)
target_link_libraries(on1x-archvirt-run PRIVATE on1x_archvirt_host)
set_target_properties(on1x-archvirt-run PROPERTIES INSTALL_RPATH "${_archvirt_libdir}")
add_executable(on1x-vmm-repl archvirt/session_host.cpp archvirt/session_console.c)
target_include_directories(on1x-vmm-repl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_library(on1x_archvirt_cache STATIC src/archvirt_cache.cpp)
target_include_directories(on1x_archvirt_cache PUBLIC include)
target_include_directories(on1x_archvirt_cache PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
# on1x_frontend owns the single MuLIB SHA implementation TU used by the cache.
target_link_libraries(on1x_archvirt_cache PRIVATE on1x_frontend)
target_sources(on1xc PRIVATE src/archvirt_driver.cpp)
target_link_libraries(on1xc PRIVATE on1x_archvirt_cache)
target_compile_definitions(on1xc PRIVATE ON1X_WITH_ARCHVIRT=1
  "ON1X_ARCHVIRT_BUILD_BIN=\"$<TARGET_FILE_DIR:on1x-archvirt-wrapper>\""
  "ON1X_ARCHVIRT_BUILD_SOURCE=\"${_archvirt_source}\""
  "ON1X_ARCHVIRT_BUILD_ARCHIVE=\"${_archvirt_runtime}\""
  "ON1X_ARCHVIRT_INCLUDE=\"${_archvirt_include}\""
  "ON1X_ARCHVIRT_PYTHON=\"${Python3_EXECUTABLE}\""
  "ON1X_ARCHVIRT_CLANG=\"${ON1X_ARCHVIRT_CLANG}\""
  "ON1X_ARCHVIRT_LLD=\"${ON1X_ARCHVIRT_LLD}\"")
add_dependencies(on1xc on1x-archvirt-wrapper on1x-archvirt-run on1x-vmm-repl on1x_archvirt_runtime)
install(TARGETS on1x-archvirt-wrapper on1x-archvirt-run on1x-vmm-repl RUNTIME DESTINATION libexec/on1x)
install(FILES "${_archvirt_runtime}" archvirt/compile.py archvirt/guest.ld archvirt/session.py
  DESTINATION share/on1x/archvirt)
install(DIRECTORY archvirt/include DESTINATION share/on1x/archvirt)

if(BUILD_TESTING)
  add_executable(on1x_archvirt_cache_tests tests/test_archvirt_cache.cpp)
  target_link_libraries(on1x_archvirt_cache_tests PRIVATE on1x_archvirt_cache)
  add_test(NAME on1x.archvirt.cache COMMAND on1x_archvirt_cache_tests "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(on1x.archvirt.cache PROPERTIES TIMEOUT 30)
  add_test(NAME on1x.archvirt.core COMMAND "${Python3_EXECUTABLE}" -B
    "${_archvirt_source}/test_core.py" --compiler "$<TARGET_FILE:on1xc>"
    --runner "$<TARGET_FILE:on1x-archvirt-run>"
    --clang "${ON1X_ARCHVIRT_CLANG}" --lld "${ON1X_ARCHVIRT_LLD}"
    --runtime-include "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    --archvirt-include "${_archvirt_include}" --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(on1x.archvirt.core PROPERTIES TIMEOUT 120)
  add_test(NAME on1x.archvirt.session_host COMMAND "${Python3_EXECUTABLE}" -IB
    "${_archvirt_source}/session_test.py" --repl "$<TARGET_FILE:on1x-vmm-repl>"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME on1x.archvirt.session_kvm COMMAND "${Python3_EXECUTABLE}" -IB
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vmm_sessions.py" "$<TARGET_FILE:on1xc>")
  set_tests_properties(on1x.archvirt.session_host on1x.archvirt.session_kvm PROPERTIES TIMEOUT 90)
  add_test(NAME on1x.addon_mcp_vmm COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/addons/mcp/test_vmm.py"
    --on1xc "$<TARGET_FILE:on1xc>" --runner "$<TARGET_FILE:on1x-archvirt-run>")
  set_tests_properties(on1x.addon_mcp_vmm PROPERTIES TIMEOUT 120)
endif()
