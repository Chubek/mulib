if(NOT ON1X_BUILD_NATIVE_SDK_REFERENCE)
  return()
endif()

if(CMAKE_CROSSCOMPILING)
  message(STATUS
    "On1x-native SDK reference extension: unsupported while cross-compiling")
  return()
endif()

set(_on1x_sdk8_sdk "${CMAKE_CURRENT_LIST_DIR}/..")
set(_on1x_sdk8_root "${_on1x_sdk8_sdk}/..")
set(_on1x_sdk8_generated "${CMAKE_CURRENT_BINARY_DIR}/sdk-native")
set(_on1x_sdk8_extension
  "${_on1x_sdk8_root}/sdk/examples/native/reference_analyzer.on1x")
set(_on1x_sdk8_positive
  "${_on1x_sdk8_root}/sdk/tests/native/fixtures/stdlib_positive.on1x")
file(GLOB _on1x_sdk8_modules CONFIGURE_DEPENDS
  "${_on1x_sdk8_root}/stdlib/sdk/*.on1x")

set(_on1x_sdk8_extension_c
  "${_on1x_sdk8_generated}/reference_analyzer.c")
add_custom_command(
  OUTPUT "${_on1x_sdk8_extension_c}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_on1x_sdk8_generated}"
  COMMAND "$<TARGET_FILE:on1xc>" --emit-c "${_on1x_sdk8_extension}"
    -o "${_on1x_sdk8_extension_c}"
  DEPENDS on1xc "${_on1x_sdk8_extension}" ${_on1x_sdk8_modules}
  VERBATIM)
set_source_files_properties("${_on1x_sdk8_extension_c}" PROPERTIES
  GENERATED TRUE
  COMPILE_DEFINITIONS "main=on1x_sdk_native_generated_main")

add_executable(on1x_sdk_native_reference_host
  "${_on1x_sdk8_extension_c}"
  "${_on1x_sdk8_sdk}/native/reference_extension/trusted_bridge.c"
  "${_on1x_sdk8_sdk}/native/reference_extension/module_adapter.c"
  "${_on1x_sdk8_sdk}/tests/native/host_profile.c")
target_include_directories(on1x_sdk_native_reference_host PRIVATE
  "${_on1x_sdk8_root}/runtime"
  "${_on1x_sdk8_sdk}/native/reference_extension"
  "${_on1x_sdk8_sdk}/include")
target_link_libraries(on1x_sdk_native_reference_host PRIVATE on1x_runtime)
if(UNIX)
  target_link_libraries(on1x_sdk_native_reference_host PRIVATE m)
endif()

set(_on1x_sdk8_positive_c
  "${_on1x_sdk8_generated}/stdlib_positive.c")
add_custom_command(
  OUTPUT "${_on1x_sdk8_positive_c}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_on1x_sdk8_generated}"
  COMMAND "$<TARGET_FILE:on1xc>" --emit-c "${_on1x_sdk8_positive}"
    -o "${_on1x_sdk8_positive_c}"
  DEPENDS on1xc "${_on1x_sdk8_positive}" ${_on1x_sdk8_modules}
  VERBATIM)
set_source_files_properties("${_on1x_sdk8_positive_c}" PROPERTIES GENERATED TRUE)
add_executable(on1x_sdk_native_stdlib_positive
  "${_on1x_sdk8_positive_c}"
  "${_on1x_sdk8_sdk}/native/reference_extension/trusted_bridge.c")
target_include_directories(on1x_sdk_native_stdlib_positive PRIVATE
  "${_on1x_sdk8_root}/runtime"
  "${_on1x_sdk8_sdk}/native/reference_extension"
  "${_on1x_sdk8_sdk}/include")
target_link_libraries(on1x_sdk_native_stdlib_positive PRIVATE on1x_runtime)
if(UNIX)
  target_link_libraries(on1x_sdk_native_stdlib_positive PRIVATE m)
endif()

add_library(on1x_sdk_native_header_cpp OBJECT
  "${_on1x_sdk8_sdk}/tests/native/header_c.c"
  "${_on1x_sdk8_sdk}/tests/native/header_cpp.cpp")
target_include_directories(on1x_sdk_native_header_cpp PRIVATE
  "${_on1x_sdk8_sdk}/native/reference_extension"
  "${_on1x_sdk8_sdk}/include")
set_target_properties(on1x_sdk_native_header_cpp PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(on1x_sdk_native_reference_host PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(on1x_sdk_native_stdlib_positive PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(on1x_sdk_native_header_cpp PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
  add_test(NAME on1x.sdk.native.host
    COMMAND on1x_sdk_native_reference_host)
  add_test(NAME on1x.sdk.native.stdlib
    COMMAND on1x_sdk_native_stdlib_positive)
  add_test(NAME on1x.sdk.native.sources
    COMMAND "${Python3_EXECUTABLE}" -B
      "${_on1x_sdk8_sdk}/tests/native/test_native_sdk.py"
      "$<TARGET_FILE:on1xc>"
      "${_on1x_sdk8_root}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/sdk-native"
      "${CMAKE_C_COMPILER}")
  set_tests_properties(
    on1x.sdk.native.host
    on1x.sdk.native.stdlib
    on1x.sdk.native.sources
    PROPERTIES TIMEOUT 180)
endif()
