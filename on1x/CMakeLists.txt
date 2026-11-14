cmake_minimum_required(VERSION 3.20)
project(On1x VERSION 0.1.0 LANGUAGES C CXX)
include(CTest)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(_libstone_default "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libstone")
if(DEFINED ENV{ON1X_LIBSTONE_ROOT} AND NOT "$ENV{ON1X_LIBSTONE_ROOT}" STREQUAL "")
  set(_libstone_default "$ENV{ON1X_LIBSTONE_ROOT}")
endif()
set(ON1X_LIBSTONE_ROOT "${_libstone_default}" CACHE PATH "LibStone source checkout")
option(ON1X_REQUIRE_LIBSTONE "Fail if LibStone generators are unavailable" OFF)
option(ON1X_USE_FALLBACK_SCANNER "Use the bootstrap scanner even when LibStone is present" OFF)
option(ON1X_WITH_EXOML "Build optional ExoML contract-verification tool" OFF)
set(ON1X_TEST_C_FLAGS "" CACHE STRING "Extra generated-program compiler/linker options (semicolon-separated)")
if(ON1X_REQUIRE_LIBSTONE AND ON1X_USE_FALLBACK_SCANNER)
  message(FATAL_ERROR "ON1X_REQUIRE_LIBSTONE and ON1X_USE_FALLBACK_SCANNER are mutually exclusive")
endif()
set(_asdl "${ON1X_LIBSTONE_ROOT}/scripts/asdl.pl")
set(_aurocks "${ON1X_LIBSTONE_ROOT}/scripts/aurocks.pl")
if(NOT ON1X_USE_FALLBACK_SCANNER AND EXISTS "${_asdl}" AND EXISTS "${_aurocks}")
  find_package(Perl REQUIRED)
  set(_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${_generated}")
  add_custom_command(
    OUTPUT "${_generated}/on1x_ast.h" "${_generated}/on1x_ast.c" "${_generated}/on1x_parser.c" "${_generated}/ppx_ast.h" "${_generated}/ppx_ast.c"
    COMMAND "${CMAKE_COMMAND}" "-DPERL=${PERL_EXECUTABLE}" "-DSCRIPTS=${ON1X_LIBSTONE_ROOT}/scripts"
      "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT=${_generated}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateScanner.cmake"
    DEPENDS "${_asdl}" "${_aurocks}" grammar/On1x.asdl grammar/On1x.g grammar/PPX.asdl
      cmake/GenerateScanner.cmake VERBATIM)
  add_library(on1x_scanner STATIC "${_generated}/on1x_parser.c" "${_generated}/on1x_ast.c" "${_generated}/ppx_ast.c" src/ppx_asdl.c)
  target_include_directories(on1x_scanner PRIVATE "${_generated}" include)
  set(ON1X_SCANNER_DESCRIPTION "LibStone ASDL+Aurocks generated frontend")
  set(ON1X_GENERATED_FRONTEND TRUE)
elseif(ON1X_REQUIRE_LIBSTONE)
  message(FATAL_ERROR "Missing LibStone generators under ${ON1X_LIBSTONE_ROOT}/scripts")
else()
  add_library(on1x_scanner STATIC src/lexer_fallback.cpp)
  target_include_directories(on1x_scanner PRIVATE include)
  set(ON1X_SCANNER_DESCRIPTION "portable fallback scanner (bootstrap only)")
endif()
message(STATUS "On1x scanner: ${ON1X_SCANNER_DESCRIPTION}")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS grammar/PPX.schema.json)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/grammar/PPX.schema.json" ON1X_SCHEMA_TEXT)
configure_file(cmake/ppx_schema.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/ppx_schema.hpp" @ONLY)
add_library(on1x_frontend STATIC src/source.cpp src/lexer.cpp src/type.cpp src/parser.cpp src/infer.cpp src/sema.cpp src/codegen.cpp
  src/sexpr.cpp src/ppx_schema.cpp src/ppx_ast.cpp src/macros.cpp src/macro_rules.cpp src/ppx.cpp src/primitives.cpp src/staging.cpp)
target_include_directories(on1x_frontend PUBLIC include)
target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(ON1X_GENERATED_FRONTEND)
  target_compile_definitions(on1x_frontend PRIVATE ON1X_ASDL_PPX=1)
endif()
target_link_libraries(on1x_frontend PUBLIC on1x_scanner)
add_executable(on1xc src/cli.cpp)
target_link_libraries(on1xc PRIVATE on1x_frontend)
add_library(on1x_runtime STATIC runtime/on1x_runtime.c)
target_include_directories(on1x_runtime PUBLIC runtime)
option(ON1X_BUILD_NATIVE_PPX "Build the self-hosted On1x PPX processors" ON)
if(ON1X_BUILD_NATIVE_PPX)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stdppx")
  file(GLOB_RECURSE _native_stdlib CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/*.on1x")
  foreach(_processor identity checks control unroll derive fold)
    set(_native_input "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/native/${_processor}.on1x")
    set(_native_c "${CMAKE_CURRENT_BINARY_DIR}/stdppx/${_processor}.c")
    add_custom_command(OUTPUT "${_native_c}"
      COMMAND "$<TARGET_FILE:on1xc>" --emit-c "${_native_input}" -o "${_native_c}"
      DEPENDS on1xc "${_native_input}" ${_native_stdlib} VERBATIM)
    add_executable(on1x_ppx_${_processor} "${_native_c}")
    target_link_libraries(on1x_ppx_${_processor} PRIVATE on1x_runtime)
    set_target_properties(on1x_ppx_${_processor} PROPERTIES OUTPUT_NAME "on1x-ppx-${_processor}" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stdppx")
  endforeach()
endif()

if(ON1X_WITH_EXOML)
  if(NOT EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/CMakeLists.txt")
    message(FATAL_ERROR "ON1X_WITH_EXOML needs a checkout at ON1X_LIBSTONE_ROOT")
  endif()
  set(MOONSTONE_BUILD_CHISEL OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_SYREN OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_SUPERSONIC OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_PLAID OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_PROLIFIC OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_EXOML_EXTRACT OFF CACHE BOOL "" FORCE)
  add_subdirectory("${ON1X_LIBSTONE_ROOT}/moonstone" "${CMAKE_CURRENT_BINARY_DIR}/libstone-moonstone" EXCLUDE_FROM_ALL)
  add_executable(on1x-bind tools/on1x-bind.cpp)
  target_link_libraries(on1x-bind PRIVATE Moonstone::ExoML)
endif()

if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(on1x_scanner_tests tests/test_scanner.cpp)
  target_link_libraries(on1x_scanner_tests PRIVATE on1x_frontend Threads::Threads)
  add_test(NAME on1x.scanner COMMAND on1x_scanner_tests)
  set_tests_properties(on1x.scanner PROPERTIES TIMEOUT 40)
  add_executable(on1x_type_tests tests/test_types.cpp)
  target_link_libraries(on1x_type_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.types COMMAND on1x_type_tests)
  set_tests_properties(on1x.types PROPERTIES TIMEOUT 40)
  add_executable(on1x_inference_tests tests/test_infer.cpp)
  target_link_libraries(on1x_inference_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.inference_engine COMMAND on1x_inference_tests)
  set_tests_properties(on1x.inference_engine PROPERTIES TIMEOUT 40)
  add_executable(on1x_closure_hir_tests tests/test_closures.cpp)
  target_link_libraries(on1x_closure_hir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.closure_hir COMMAND on1x_closure_hir_tests)
  set_tests_properties(on1x.closure_hir PROPERTIES TIMEOUT 40)
  add_executable(on1x_tuple_hir_tests tests/test_tuples.cpp)
  target_link_libraries(on1x_tuple_hir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.tuple_hir COMMAND on1x_tuple_hir_tests)
  set_tests_properties(on1x.tuple_hir PROPERTIES TIMEOUT 40)
  if(ON1X_GENERATED_FRONTEND)
    # Differential scanner regression: the generated Aurocks scanner must
    # match independent fallback tokenization, including byte offsets.
    add_library(on1x_reference_scanner STATIC src/lexer_fallback.cpp)
    target_include_directories(on1x_reference_scanner PRIVATE include)
    add_executable(on1x_reference_tokens tests/reference_tokens.cpp src/lexer.cpp src/source.cpp)
    target_include_directories(on1x_reference_tokens PRIVATE include)
    target_link_libraries(on1x_reference_tokens PRIVATE on1x_reference_scanner)
    add_executable(on1x_generated_tokens tests/reference_tokens.cpp)
    target_link_libraries(on1x_generated_tokens PRIVATE on1x_frontend)
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(_test_c_flags)
  foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
    list(APPEND _test_c_flags "--cflag=${_flag}")
  endforeach()
  add_test(NAME on1x.compiler COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compiler.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/compiler" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.limits COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_limits.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/limits" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.functions COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_functions.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/functions" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.json COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_json.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/json" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.inference COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inference.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/inference" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.closures COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_closures.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/closures" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.tuples COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tuples.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/tuples" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.parameter_patterns COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parameter_patterns.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/parameter-patterns" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  set_tests_properties(on1x.compiler on1x.limits on1x.functions on1x.json on1x.inference on1x.closures on1x.tuples on1x.parameter_patterns PROPERTIES TIMEOUT 120)
  set_tests_properties(on1x.json PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME on1x.poly COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_poly.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/poly" "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.poly PROPERTIES TIMEOUT 120)
  add_test(NAME on1x.poly_extended COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_poly_extended.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/poly-extended" "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.poly_extended PROPERTIES TIMEOUT 120)
  add_executable(on1x_meta_tests tests/test_meta.cpp)
  target_link_libraries(on1x_meta_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.meta_hir COMMAND on1x_meta_tests)
  foreach(_meta_test macros ppx staging selfhost)
    add_test(NAME on1x.${_meta_test} COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${_meta_test}.py" "$<TARGET_FILE:on1xc>"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/${_meta_test}" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.${_meta_test} PROPERTIES TIMEOUT 180)
  endforeach()
  if(ON1X_GENERATED_FRONTEND)
    add_test(NAME on1x.scanner_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/scanner_parity.py"
      "$<TARGET_FILE:on1x_generated_tokens>" "$<TARGET_FILE:on1x_reference_tokens>"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/parity")
    set_tests_properties(on1x.scanner_parity PROPERTIES TIMEOUT 120)
  endif()
endif()
