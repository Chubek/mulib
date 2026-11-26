# Native execution fixtures for explicit-link stdlib companions. The compiler
# and the package sources still install when these host-only tests are absent.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING
    OR NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  return()
endif()

set(_probe_work "${CMAKE_CURRENT_BINARY_DIR}/test-work/stdlib-probe")
file(MAKE_DIRECTORY "${_probe_work}/daemon")

add_test(NAME on1x.stdlib.http_probe COMMAND "${Python3_EXECUTABLE}" -B
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_http_probe.py"
  --on1xc "$<TARGET_FILE:on1xc>" --cc "${CMAKE_C_COMPILER}"
  --scratch "${_probe_work}" --package-tests ${_test_c_flags})
set_tests_properties(on1x.stdlib.http_probe PROPERTIES TIMEOUT 180)
add_test(NAME on1x.stdlib.http_probe_components COMMAND "${Python3_EXECUTABLE}" -B
  "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/http/probe/tests/run.py"
  --on1xc "$<TARGET_FILE:on1xc>" --cc "${CMAKE_C_COMPILER}"
  --work-root "${_probe_work}" ${_test_c_flags})
set_tests_properties(on1x.stdlib.http_probe_components PROPERTIES TIMEOUT 180)

foreach(_package IN ITEMS elvi html)
  add_test(NAME on1x.stdlib.${_package} COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/${_package}/tests/run.py"
    "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.stdlib.${_package} PROPERTIES
    ENVIRONMENT "TMPDIR=${_probe_work}" TIMEOUT 180)
endforeach()

function(on1x_probe_native_test target)
  add_executable(${target} ${ARGN})
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../kernels")
  target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
  target_compile_options(${target} PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
endfunction()

on1x_probe_native_test(on1x_stdlib_kron_test stdlib/kron/tests/test_kron.c
  stdlib/kron/runtime.c stdlib/kron/kernels.c)
target_include_directories(on1x_stdlib_kron_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/kron")
target_compile_definitions(on1x_stdlib_kron_test PRIVATE BASIS_KRON_TEST_WRAP)
target_link_options(on1x_stdlib_kron_test PRIVATE
  -Wl,--wrap=fsync,--wrap=malloc,--wrap=calloc,--wrap=realloc)
add_test(NAME on1x.stdlib.kron_native COMMAND on1x_stdlib_kron_test)

find_package(Threads REQUIRED)
on1x_probe_native_test(on1x_stdlib_daemon_test stdlib/daemon/test_supervisor.c
  stdlib/daemon/supervisor.c stdlib/daemon/lifecycle_kernels.c)
target_link_libraries(on1x_stdlib_daemon_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
target_link_options(on1x_stdlib_daemon_test PRIVATE
  -Wl,--wrap=malloc,--wrap=calloc,--wrap=execve,--wrap=waitpid)
add_test(NAME on1x.stdlib.daemon_supervisor COMMAND on1x_stdlib_daemon_test "${_probe_work}/daemon")

on1x_probe_native_test(on1x_stdlib_daemon_unsupported_test stdlib/daemon/test_supervisor.c
  stdlib/daemon/supervisor.c)
target_compile_definitions(on1x_stdlib_daemon_unsupported_test PRIVATE MULIB_DAEMON_NO_POSIX)
add_test(NAME on1x.stdlib.daemon_supervisor_unsupported COMMAND on1x_stdlib_daemon_unsupported_test)

on1x_probe_native_test(on1x_stdlib_probe_context_test stdlib/http/probe/tests/context.c
  stdlib/http/probe/context.c)
add_test(NAME on1x.stdlib.http_probe_context COMMAND on1x_stdlib_probe_context_test)

set_tests_properties(on1x.stdlib.kron_native on1x.stdlib.daemon_supervisor
  on1x.stdlib.daemon_supervisor_unsupported on1x.stdlib.http_probe_context
  PROPERTIES TIMEOUT 60)
