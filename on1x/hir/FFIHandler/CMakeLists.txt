# Mandatory scalar descriptor implementation once the migration is applied.
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
# Only the existing clamp ABI is connected to the On1x loader in this migration.
set(_ffi_specs "${CMAKE_CURRENT_SOURCE_DIR}/../../stdlib/ffi/mulib/clamp.ffi")
add_custom_command(OUTPUT "${_ffi_parser}"
  COMMAND "${PERL_EXECUTABLE}" "${_ffi_driver}" --mode=interpret -T python3 -o "${_ffi_parser}" "${CMAKE_CURRENT_SOURCE_DIR}/FFIHandler.llsynth"
  DEPENDS FFIHandler.llsynth ${_ffi_generator_sources} "${PERL_EXECUTABLE}" VERBATIM)
add_custom_command(OUTPUT "${_ffi_header}" "${_ffi_manifest}"
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/ffic.py" --parser "${_ffi_parser}" --header "${_ffi_header}" --manifest "${_ffi_manifest}" ${_ffi_specs}
  DEPENDS "${_ffi_parser}" tools/ffic.py ${_ffi_specs} "${CMAKE_CURRENT_SOURCE_DIR}/../../include/on1x/ffi_scalar.hpp" VERBATIM)
add_custom_target(on1x_ffi_tables DEPENDS "${_ffi_header}" "${_ffi_manifest}")
add_dependencies(on1x_frontend on1x_ffi_tables)
target_sources(on1x_frontend PRIVATE ffi.cpp)
target_include_directories(on1x_frontend PRIVATE "${_ffi_gen}")
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME on1x.ffihandler.scalar
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ffi.py" --parser "${_ffi_parser}" --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(on1x.ffihandler.scalar PROPERTIES TIMEOUT 90)
endif()
