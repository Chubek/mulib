if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING
    OR NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  return()
endif()

find_package(Threads REQUIRED)
set(_probe_example "${CMAKE_CURRENT_SOURCE_DIR}/examples/http-probe")
set(_probe_stdlib "${CMAKE_CURRENT_SOURCE_DIR}/stdlib")
set(_probe_source "${CMAKE_CURRENT_BINARY_DIR}/generated_http_probe.c")
file(GLOB_RECURSE _probe_modules CONFIGURE_DEPENDS
  "${_probe_stdlib}/elvi/*.on1x" "${_probe_stdlib}/html/*.on1x"
  "${_probe_stdlib}/http/probe/*.on1x" "${_probe_stdlib}/core/*.on1x")
list(FILTER _probe_modules EXCLUDE REGEX "/tests/")
add_custom_command(OUTPUT "${_probe_source}"
  COMMAND $<TARGET_FILE:on1xc> --emit-c --stdlib "${_probe_stdlib}"
    "${_probe_example}/main.on1x" -o "${_probe_source}"
  DEPENDS on1xc "${_probe_example}/main.on1x" "${_probe_stdlib}/pervasive.on1x"
    ${_probe_modules}
  VERBATIM)

add_executable(on1x_http_probe_example EXCLUDE_FROM_ALL
  "${_probe_source}" "${_probe_example}/fixture.c" "${_probe_example}/kernels.c"
  "${_probe_stdlib}/elvi/runtime.c" "${_probe_stdlib}/elvi/bridge.c"
  "${_probe_stdlib}/elvi/capture.c" "${_probe_stdlib}/elvi/live.c"
  "${_probe_stdlib}/html/runtime.c")
target_include_directories(on1x_http_probe_example PRIVATE
  "${_probe_stdlib}" "${CMAKE_CURRENT_SOURCE_DIR}/../kernels")
target_compile_definitions(on1x_http_probe_example PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(on1x_http_probe_example PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(on1x_http_probe_example PRIVATE on1x_runtime Threads::Threads ${CMAKE_DL_LIBS} m)
