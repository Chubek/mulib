# Build-host tools only: the frontend links immutable tables, not Cephyr or Python.
find_package(Perl REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_ffi_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
if(NOT EXISTS "${_ffi_driver}")
  message(FATAL_ERROR "FFIHandler requires LibStone LLSynth: ${_ffi_driver}")
endif()
if(On1x_SOURCE_DIR STREQUAL On1x_BINARY_DIR)
  message(FATAL_ERROR "Standard FFI recipes must be generated in a separate build directory")
endif()
if(ON1X_STDFFI_SPEC_FILES)
  message(FATAL_ERROR "ON1X_STDFFI_SPEC_FILES was removed; use MULIB_KERNELS_PATH to select C header roots")
endif()

set(ON1X_STDFFI_CEPHYR_LIBRARY "" CACHE FILEPATH "Build-host Cephyr shared library for FFI generation")
set(_ffi_cephyr "${ON1X_LIBSTONE_ROOT}/cornerstone/Cephyr")
set(ON1X_STDFFI_CEPHYR_CONFIG "${_ffi_cephyr}/Cephyr.toml" CACHE FILEPATH
  "Build-host Cephyr configuration for FFI generation")
set(_ffi_library "${ON1X_STDFFI_CEPHYR_LIBRARY}")
if(NOT _ffi_library)
  set(_ffi_library "$ENV{CEPHYR_LIBRARY}")
endif()
if(NOT _ffi_library AND NOT CMAKE_CROSSCOMPILING)
  find_package(Cephyr CONFIG QUIET)
  if(TARGET Cephyr::Cephyr)
    get_target_property(_ffi_cephyr_type Cephyr::Cephyr TYPE)
    if(_ffi_cephyr_type STREQUAL "SHARED_LIBRARY")
      set(_ffi_library "$<TARGET_FILE:Cephyr::Cephyr>")
    endif()
  endif()
endif()
if(NOT _ffi_library)
  message(FATAL_ERROR "FFIHandler requires a build-host libcephyr. Set ON1X_STDFFI_CEPHYR_LIBRARY, CEPHYR_LIBRARY, or Cephyr_DIR.")
endif()
if(NOT _ffi_library MATCHES "^\\$<")
  get_filename_component(_ffi_library "${_ffi_library}" ABSOLUTE BASE_DIR "${On1x_SOURCE_DIR}")
  if(NOT EXISTS "${_ffi_library}" OR IS_DIRECTORY "${_ffi_library}")
    message(FATAL_ERROR "Not a build-host Cephyr shared library: ${_ffi_library}")
  endif()
endif()
get_filename_component(_ffi_config "${ON1X_STDFFI_CEPHYR_CONFIG}" ABSOLUTE BASE_DIR "${On1x_SOURCE_DIR}")
if(NOT EXISTS "${_ffi_config}" OR IS_DIRECTORY "${_ffi_config}")
  message(FATAL_ERROR "Not a build-host Cephyr configuration: ${_ffi_config}")
endif()

set(_ffi_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_ffi_grammar "${CMAKE_CURRENT_SOURCE_DIR}/FFIHandler.llsynth")
set(_ffi_parser "${_ffi_generated}/FFIHandler.py")
set(_ffi_tables "${_ffi_generated}/on1x_stdffi_tables.hpp")
set(_ffi_bundle "${On1x_BINARY_DIR}/stdffi/mulib")
file(MAKE_DIRECTORY "${_ffi_generated}")
get_filename_component(_ffi_scripts "${_ffi_driver}" DIRECTORY)
file(GLOB _ffi_generator_sources CONFIGURE_DEPENDS "${_ffi_scripts}/*.pl" "${_ffi_scripts}/*.pm")
add_custom_command(OUTPUT "${_ffi_parser}"
  COMMAND "${PERL_EXECUTABLE}" "${_ffi_driver}" --mode=interpret -T python3
    -o "${_ffi_parser}" "${_ffi_grammar}"
  DEPENDS "${_ffi_grammar}" "${PERL_EXECUTABLE}" "${_ffi_driver}" ${_ffi_generator_sources}
  COMMENT "Generate the FFIHandler parser using LibStone LLSynth" VERBATIM)

# Scan on every build so environment changes, added files, removed files and
# transitive includes cannot evade the generator's content-based cache.
add_custom_target(on1x_stdffi_tables
  COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${_ffi_cephyr}/python"
    "${Python3_EXECUTABLE}" -sB "${CMAKE_CURRENT_SOURCE_DIR}/ffispec-gen.py"
    --cephyr-library "${_ffi_library}" --cephyr-resource-root "${_ffi_cephyr}"
    --config "${_ffi_config}" --output-dir "${_ffi_bundle}"
  COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/compile_recipes.py"
    --parser "${_ffi_parser}" --generated-index "${_ffi_bundle}/index.json" --output "${_ffi_tables}"
  BYPRODUCTS "${_ffi_tables}" "${_ffi_bundle}/index.json"
  DEPENDS "${_ffi_parser}"
  COMMENT "Scan MuLIB kernel roots and compile changed FFI recipes" VERBATIM)
add_dependencies(on1x_frontend on1x_stdffi_tables)
target_sources(on1x_frontend PRIVATE stdffi.cpp selected_recipe.cpp)
target_include_directories(on1x_frontend BEFORE PRIVATE "${_ffi_generated}")
target_compile_definitions(on1x_frontend PRIVATE
  ON1X_BUILT_STDFFI_ROOT="${_ffi_bundle}/recipes")
install(DIRECTORY "${_ffi_bundle}/recipes/" DESTINATION share/on1x/stdffi/mulib
  OPTIONAL FILES_MATCHING PATTERN "*.ffi")

if(BUILD_TESTING)
  add_test(NAME on1x.stdffi.cephyr_generator COMMAND "${CMAKE_COMMAND}" -E env
    "PYTHONPATH=${_ffi_cephyr}/python" "CEPHYR_LIBRARY=${_ffi_library}"
    "ON1X_STDFFI_CEPHYR_RESOURCE_ROOT=${_ffi_cephyr}"
    "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_ffispec_gen.py")
  add_test(NAME on1x.stdffi.recipe_compiler
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler.py"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${_ffi_parser}" "${CMAKE_CURRENT_BINARY_DIR}")

  # Exercise every wire directive with an isolated test fixture; never stage
  # fixture recipes in the production discovery directory.
  set(_ffi_test "${_ffi_generated}/test")
  set(_ffi_test_tables "${_ffi_test}/on1x_stdffi_tables.hpp")
  add_custom_command(OUTPUT "${_ffi_test_tables}"
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler.py"
      --fixture-index "${CMAKE_CURRENT_SOURCE_DIR}" "${_ffi_test}"
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/compile_recipes.py"
      --parser "${_ffi_parser}" --generated-index "${_ffi_test}/index.json" --output "${_ffi_test_tables}"
    DEPENDS "${_ffi_parser}" compile_recipes.py tests/compiler.py tests/directives.ffi
      tests/mulib-ffifixture.h
    COMMENT "Compile FFI directive test fixtures" VERBATIM)
  add_custom_target(on1x_stdffi_test_tables DEPENDS "${_ffi_test_tables}")
  foreach(_ffi_test_name IN ITEMS directives recipes)
    add_executable(on1x_stdffi_${_ffi_test_name}_tests tests/${_ffi_test_name}.cpp
      stdffi.cpp selected_recipe.cpp)
    add_dependencies(on1x_stdffi_${_ffi_test_name}_tests on1x_stdffi_test_tables)
    target_include_directories(on1x_stdffi_${_ffi_test_name}_tests BEFORE PRIVATE "${_ffi_test}")
    target_link_libraries(on1x_stdffi_${_ffi_test_name}_tests PRIVATE on1x_frontend)
  endforeach()
  add_test(NAME on1x.stdffi.mir_directives COMMAND on1x_stdffi_directives_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/directives.on1x")
  set_tests_properties(on1x.stdffi.mir_directives PROPERTIES
    ENVIRONMENT "MULIB_FOREIGNLIB_PATH=${_ffi_test}/recipes")
  add_test(NAME on1x.stdffi.checked_recipes COMMAND on1x_stdffi_recipes_tests)
endif()
