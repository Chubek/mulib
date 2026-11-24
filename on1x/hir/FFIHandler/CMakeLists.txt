# The generated parser and Python validator are build-time tools only.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Perl REQUIRED)
set(_ffi_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
if(NOT EXISTS "${_ffi_driver}")
  message(FATAL_ERROR "FFIHandler requires real LibStone LLSynth at ON1X_LIBSTONE_ROOT")
endif()
file(GLOB _ffi_generator_sources CONFIGURE_DEPENDS "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pl" "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pm")
set(_ffi_gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_ffi_gen}")
set(_ffi_parser "${_ffi_gen}/FFIHandler.py")
set(_ffi_header "${_ffi_gen}/on1x_ffi_rules.hpp")
set(_ffi_manifest "${_ffi_gen}/ffi-manifest.json")
set(_ffi_compiler "${CMAKE_CURRENT_SOURCE_DIR}/tools/ffic.py")
set(ON1X_FFI_CATALOG "${CMAKE_CURRENT_SOURCE_DIR}/../../stdffi/mulib" CACHE PATH
    "Target-qualified generated MuLIB FFI catalogue (with index.json)")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ffi_cpu)
if(_ffi_cpu STREQUAL "amd64")
  set(_ffi_cpu "x86_64")
elseif(_ffi_cpu STREQUAL "arm64")
  set(_ffi_cpu "aarch64")
endif()
set(ON1X_FFI_TARGET "${_ffi_cpu}-linux-gnu" CACHE STRING
    "Target identity required in the generated FFI catalogue")
if(NOT ON1X_FFI_TARGET MATCHES "^(x86_64|aarch64|riscv64)-linux-gnu$")
  message(FATAL_ERROR "Unsupported FFI target: ${ON1X_FFI_TARGET}")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT ON1X_FFI_TARGET STREQUAL "${_ffi_cpu}-linux-gnu")
  message(FATAL_ERROR "FFI catalogue target ${ON1X_FFI_TARGET} must match the Linux LP64 compiler target")
endif()

file(GLOB _ffi_specs CONFIGURE_DEPENDS "${ON1X_FFI_CATALOG}/*.ffi")
list(SORT _ffi_specs)
list(LENGTH _ffi_specs _ffi_count)
set(_ffi_catalogue_index "${ON1X_FFI_CATALOG}/index.json")
if(_ffi_count LESS 1 OR _ffi_count GREATER 8192 OR NOT EXISTS "${_ffi_catalogue_index}")
  message(FATAL_ERROR "FFIHandler requires an indexed generated catalogue at ${ON1X_FFI_CATALOG}")
endif()
add_custom_command(OUTPUT "${_ffi_parser}"
  # LLSynth emits some tables in Perl hash order. Stabilize this build tool's
  # output so rebuilding the parser does not change descriptor provenance.
  COMMAND "${CMAKE_COMMAND}" -E env PERL_HASH_SEED=0 PERL_PERTURB_KEYS=0
    "${PERL_EXECUTABLE}" "${_ffi_driver}" --mode=interpret -T python3 -o "${_ffi_parser}" "${CMAKE_CURRENT_SOURCE_DIR}/FFIHandler.llsynth"
  DEPENDS FFIHandler.llsynth ${_ffi_generator_sources} "${PERL_EXECUTABLE}" VERBATIM)
set(_ffi_unit_headers)
set(_ffi_unit_manifests)
set(_ffi_offset 0)
set(_ffi_number 0)
while(_ffi_offset LESS _ffi_count)
  list(SUBLIST _ffi_specs ${_ffi_offset} 8 _ffi_slice)
  set(_ffi_unit "unit_${_ffi_number}")
  set(_ffi_unit_header "${_ffi_gen}/${_ffi_unit}.hpp")
  set(_ffi_unit_manifest "${_ffi_gen}/${_ffi_unit}.json")
  set(_ffi_unit_stamp "${_ffi_gen}/${_ffi_unit}.sources")
  # The source list is a dependency too: removing a prerequisite otherwise
  # can leave Make/Ninja with a stale header even after CMake reconfiguration.
  file(GENERATE OUTPUT "${_ffi_unit_stamp}" CONTENT "${_ffi_slice}\n")
  add_custom_command(OUTPUT "${_ffi_unit_header}" "${_ffi_unit_manifest}"
    COMMAND "${Python3_EXECUTABLE}" -B "${_ffi_compiler}" --parser "${_ffi_parser}"
      --target "${ON1X_FFI_TARGET}" --canonical-files --unit "${_ffi_unit}" --header "${_ffi_unit_header}"
      --manifest "${_ffi_unit_manifest}" ${_ffi_slice}
    DEPENDS "${_ffi_parser}" "${_ffi_compiler}" "${_ffi_unit_stamp}" ${_ffi_slice} VERBATIM)
  list(APPEND _ffi_unit_headers "${_ffi_unit_header}")
  list(APPEND _ffi_unit_manifests "${_ffi_unit_manifest}")
  math(EXPR _ffi_offset "${_ffi_offset}+8")
  math(EXPR _ffi_number "${_ffi_number}+1")
endwhile()
set(_ffi_registry_stamp "${_ffi_gen}/registry.sources")
file(GENERATE OUTPUT "${_ffi_registry_stamp}" CONTENT "${ON1X_FFI_TARGET}\n${_ffi_unit_manifests}\n")
add_custom_command(OUTPUT "${_ffi_header}" "${_ffi_manifest}"
  COMMAND "${Python3_EXECUTABLE}" -B "${_ffi_compiler}" --index --target "${ON1X_FFI_TARGET}"
    --catalogue-index "${_ffi_catalogue_index}" --header "${_ffi_header}" --manifest "${_ffi_manifest}"
    ${_ffi_unit_manifests}
  DEPENDS "${_ffi_compiler}" "${_ffi_catalogue_index}" "${_ffi_registry_stamp}"
    ${_ffi_unit_headers} ${_ffi_unit_manifests} VERBATIM)
add_custom_target(on1x_ffi_tables DEPENDS "${_ffi_header}" "${_ffi_manifest}")
add_dependencies(on1x_frontend on1x_ffi_tables)
target_sources(on1x_frontend PRIVATE ffi.cpp)
# Merge dependencies: W06 owns the checked-HIR ingress, W07 the C11 emitter.
# A descriptor-only checkout has neither interface; a checkout exposing an
# interface must compile its implementation rather than fall back to legacy.
set(_ffi_hir_api "${CMAKE_CURRENT_SOURCE_DIR}/../../include/on1x/ffi_hir.hpp")
if(EXISTS "${_ffi_hir_api}" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/verify.cpp")
  target_sources(on1x_frontend PRIVATE verify.cpp)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lower.hpp" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lower.cpp")
  target_sources(on1x_frontend PRIVATE lower.cpp)
endif()
target_include_directories(on1x_frontend PRIVATE "${_ffi_gen}")
install(FILES ${_ffi_specs} "${_ffi_catalogue_index}" DESTINATION share/on1x/stdffi/mulib)
install(FILES "${_ffi_manifest}" DESTINATION share/on1x/stdffi)
set(_ffi_tools "${CMAKE_CURRENT_SOURCE_DIR}/../../tools")
if(EXISTS "${_ffi_tools}/on1x_ffi_native.py" OR EXISTS "${_ffi_tools}/on1x_ffi_trust.py")
  install(FILES "${_ffi_tools}/on1x_ffi_native.py" "${_ffi_tools}/on1x_ffi_trust.py"
    DESTINATION share/on1x/tools)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME on1x.ffihandler.scalar
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ffi.py"
      --parser "${_ffi_parser}" --catalogue "${ON1X_FFI_CATALOG}" --work "${CMAKE_CURRENT_BINARY_DIR}"
      --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(on1x.ffihandler.scalar PROPERTIES TIMEOUT 90)
  add_test(NAME on1x.ffihandler.rich_discovery
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rich.py"
      --parser "${_ffi_parser}" --work "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(on1x.ffihandler.rich_discovery PROPERTIES TIMEOUT 90)
  if(ON1X_FFI_TARGET STREQUAL "x86_64-linux-gnu" AND
     EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_hir.cpp" AND
     EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lower.cpp")
    add_executable(on1x_ffi_check_hir tests/check_hir.cpp)
    target_link_libraries(on1x_ffi_check_hir PRIVATE on1x_frontend on1x_mir_vm)
    add_test(NAME on1x.ffihandler.checked_hir
      COMMAND on1x_ffi_check_hir "${CMAKE_CURRENT_BINARY_DIR}/check-hir")
    set_tests_properties(on1x.ffihandler.checked_hir PROPERTIES TIMEOUT 90)
  endif()
endif()
