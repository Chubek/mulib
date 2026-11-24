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
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ffi_cpu)
if(_ffi_cpu STREQUAL "amd64")
  set(_ffi_cpu "x86_64")
elseif(_ffi_cpu STREQUAL "arm64")
  set(_ffi_cpu "aarch64")
endif()
set(ON1X_FFI_TARGET "${_ffi_cpu}-linux-gnu" CACHE STRING
    "Target identity required in the generated FFI catalogue")
option(ON1X_FFI_CROSS_C11 "Build an AMD64 frontend with a cross-target checked-C11 FFI catalogue" OFF)
if(NOT ON1X_FFI_TARGET MATCHES "^(x86_64|aarch64|riscv64)-linux-gnu$")
  message(FATAL_ERROR "Unsupported FFI target: ${ON1X_FFI_TARGET}")
endif()
if(ON1X_FFI_CROSS_C11 AND NOT (_ffi_cpu STREQUAL "x86_64" AND
   ON1X_FFI_TARGET MATCHES "^(aarch64|riscv64)-linux-gnu$"))
  message(FATAL_ERROR "ON1X_FFI_CROSS_C11 requires an AMD64 frontend and an AArch64 or RV64IM LP64 catalogue")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   (NOT ON1X_FFI_CROSS_C11 AND NOT ON1X_FFI_TARGET STREQUAL "${_ffi_cpu}-linux-gnu"))
  message(FATAL_ERROR "FFI catalogue target ${ON1X_FFI_TARGET} must match the Linux LP64 compiler target")
endif()

# A toolchain file can name a processor without selecting it in the compilers.
# In particular the catalogue's RV64 LP64 is not LP64D or LP64F.
if(_ffi_cpu STREQUAL "x86_64")
  set(_ffi_arch_condition "defined(__x86_64__)")
elseif(_ffi_cpu STREQUAL "aarch64")
  set(_ffi_arch_condition "defined(__aarch64__)")
else()
  set(_ffi_arch_condition "defined(__riscv) && __riscv_xlen == 64 && defined(__riscv_float_abi_soft)")
endif()
function(_ffi_check_compilers)
  include(CheckCSourceCompiles)
  include(CheckCXXSourceCompiles)
  # Test the C and C++ compilers without linking or running a target executable.
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_REQUIRED_QUIET TRUE)
  set(_probe "
#if !defined(__linux__) || !defined(__LP64__) || !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ || !(${_ffi_arch_condition})
#error wrong FFI ABI
#endif
int main(void) { return 0; }")
  unset(_on1x_ffi_c_abi CACHE)
  unset(_on1x_ffi_cxx_abi CACHE)
  check_c_source_compiles("${_probe}" _on1x_ffi_c_abi)
  check_cxx_source_compiles("${_probe}" _on1x_ffi_cxx_abi)
  if(NOT _on1x_ffi_c_abi OR NOT _on1x_ffi_cxx_abi)
    message(FATAL_ERROR "C and C++ compilers must both target the FFI ABI ${ON1X_FFI_TARGET} (RV64 requires soft-float LP64)")
  endif()
endfunction()

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." _ffi_on1x_root)
set(_ffi_catalogue_root "${_ffi_on1x_root}/stdffi/mulib")
set(ON1X_FFI_CATALOG "${_ffi_catalogue_root}/${ON1X_FFI_TARGET}" CACHE PATH
    "Exact generated MuLIB FFI package directory (with index.json)")
get_filename_component(_ffi_catalogue "${ON1X_FFI_CATALOG}" ABSOLUTE)
file(REAL_PATH "${_ffi_catalogue_root}" _ffi_catalogue_root_real)
file(REAL_PATH "${_ffi_catalogue}" _ffi_catalogue_real)
foreach(_ffi_root IN ITEMS "${_ffi_catalogue_root}" "${_ffi_catalogue_root_real}")
  foreach(_ffi_path IN ITEMS "${_ffi_catalogue}" "${_ffi_catalogue_real}")
    cmake_path(IS_PREFIX _ffi_root "${_ffi_path}" NORMALIZE _ffi_in_tree)
    if(_ffi_in_tree AND (NOT _ffi_path STREQUAL "${_ffi_root}/${ON1X_FFI_TARGET}"
                         OR NOT _ffi_catalogue STREQUAL _ffi_catalogue_real))
      message(FATAL_ERROR "FFI catalogue must use the real ${_ffi_catalogue_root}/${ON1X_FFI_TARGET} directory")
    endif()
  endforeach()
endforeach()
if(IS_SYMLINK "${_ffi_catalogue}")
  message(FATAL_ERROR "FFI catalogue directory cannot be a symlink: ${_ffi_catalogue}")
endif()

file(GLOB _ffi_specs CONFIGURE_DEPENDS "${_ffi_catalogue}/*.ffi")
list(SORT _ffi_specs)
list(LENGTH _ffi_specs _ffi_count)
set(_ffi_catalogue_index "${_ffi_catalogue}/index.json")
if(_ffi_count LESS 1 OR _ffi_count GREATER 8192 OR NOT EXISTS "${_ffi_catalogue_index}"
   OR IS_DIRECTORY "${_ffi_catalogue_index}" OR IS_SYMLINK "${_ffi_catalogue_index}")
  message(FATAL_ERROR "FFIHandler requires an indexed ${ON1X_FFI_TARGET} catalogue at ${_ffi_catalogue}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ffi_catalogue_index}")
file(SIZE "${_ffi_catalogue_index}" _ffi_index_size)
if(_ffi_index_size LESS 1 OR _ffi_index_size GREATER 67108864)
  message(FATAL_ERROR "FFI catalogue index must be nonempty and within the byte limit: ${_ffi_catalogue_index}")
endif()
file(READ "${_ffi_catalogue_index}" _ffi_index_json)
string(JSON _ffi_index_target ERROR_VARIABLE _ffi_index_error GET "${_ffi_index_json}" target)
if(_ffi_index_error OR NOT _ffi_index_target STREQUAL ON1X_FFI_TARGET)
  message(FATAL_ERROR "FFI catalogue index target mismatch: expected ${ON1X_FFI_TARGET} in ${_ffi_catalogue_index}")
endif()
unset(_ffi_index_json)
_ffi_check_compilers()
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
  file(GENERATE OUTPUT "${_ffi_unit_stamp}" CONTENT "${ON1X_FFI_TARGET}\n${_ffi_slice}\n")
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
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ffi.cpp" TARGET_DIRECTORY on1x_frontend
  PROPERTIES COMPILE_DEFINITIONS ON1X_FFI_CONFIGURED_TARGET="${ON1X_FFI_TARGET}")
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
install(FILES ${_ffi_specs} "${_ffi_catalogue_index}"
  DESTINATION "share/on1x/stdffi/mulib/${ON1X_FFI_TARGET}")
install(FILES "${_ffi_manifest}" DESTINATION "share/on1x/stdffi/${ON1X_FFI_TARGET}")
set(_ffi_tools "${CMAKE_CURRENT_SOURCE_DIR}/../../tools")
if(EXISTS "${_ffi_tools}/on1x_ffi_native.py" OR EXISTS "${_ffi_tools}/on1x_ffi_trust.py")
  install(FILES "${_ffi_tools}/on1x_ffi_native.py" "${_ffi_tools}/on1x_ffi_trust.py"
    "${_ffi_tools}/on1x_ffi_aarch64.py" "${_ffi_tools}/on1x_ffi_rv64.py"
    DESTINATION share/on1x/tools)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  if(ON1X_FFI_TARGET STREQUAL "x86_64-linux-gnu")
    add_test(NAME on1x.ffihandler.scalar
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ffi.py"
        --parser "${_ffi_parser}" --catalogue "${_ffi_catalogue}" --work "${CMAKE_CURRENT_BINARY_DIR}"
        --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}")
    set_tests_properties(on1x.ffihandler.scalar PROPERTIES TIMEOUT 90)
  endif()
  add_test(NAME on1x.ffihandler.catalogue_inventory
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ffi_inventory.py")
  set_tests_properties(on1x.ffihandler.catalogue_inventory PROPERTIES TIMEOUT 90)
  if(NOT ON1X_FFI_CROSS_C11)
    add_test(NAME on1x.ffihandler.catalogue_cmake
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_catalogue_cmake.py"
        --work "${CMAKE_CURRENT_BINARY_DIR}" --cmake "${CMAKE_COMMAND}"
        --libstone-root "${ON1X_LIBSTONE_ROOT}"
        --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
        "--cflags=${CMAKE_C_FLAGS}" "--cxxflags=${CMAKE_CXX_FLAGS}" --target "${ON1X_FFI_TARGET}")
    set_tests_properties(on1x.ffihandler.catalogue_cmake PROPERTIES TIMEOUT 180)
  endif()
  if(ON1X_FFI_CROSS_C11 AND ON1X_FFI_TARGET STREQUAL "aarch64-linux-gnu")
    set(ON1X_FFI_AARCH64_TEST_CC "" CACHE FILEPATH "AArch64 test C driver with static Linux GNU libc")
    set(ON1X_FFI_AARCH64_TEST_QEMU "" CACHE FILEPATH "Explicit trusted AArch64 user-mode QEMU for testing")
    if(ON1X_FFI_AARCH64_TEST_CC OR ON1X_FFI_AARCH64_TEST_QEMU)
      if(NOT IS_ABSOLUTE "${ON1X_FFI_AARCH64_TEST_CC}" OR NOT IS_ABSOLUTE "${ON1X_FFI_AARCH64_TEST_QEMU}")
        message(FATAL_ERROR "AAPCS64 qualification test requires absolute CC and QEMU paths")
      endif()
      set(_ffi_aarch64_provider_args)
      if(ON1X_WITH_CEPHYR)
        list(APPEND _ffi_aarch64_provider_args --embedded-cephyr)
      endif()
      add_test(NAME on1x.ffihandler.aarch64_c11
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/ffi_aarch64_toolchain.py"
          --on1xc "$<TARGET_FILE:on1xc>" --cc "${ON1X_FFI_AARCH64_TEST_CC}"
          --qemu "${ON1X_FFI_AARCH64_TEST_QEMU}" --host-cc "${CMAKE_C_COMPILER}"
          --work "${CMAKE_CURRENT_BINARY_DIR}" ${_ffi_aarch64_provider_args})
      set_tests_properties(on1x.ffihandler.aarch64_c11 PROPERTIES TIMEOUT 300)
    endif()
  endif()
  add_test(NAME on1x.ffihandler.rich_discovery
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rich.py"
      --parser "${_ffi_parser}" --work "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(on1x.ffihandler.rich_discovery PROPERTIES TIMEOUT 90)
  add_test(NAME on1x.ffihandler.rich_plan_validation
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_plan.py"
      --parser "${_ffi_parser}")
  set_tests_properties(on1x.ffihandler.rich_plan_validation PROPERTIES TIMEOUT 90)
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
