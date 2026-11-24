# Only the build uses LLSynth or Python. The frontend links compiled data.
find_package(Perl QUIET)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_ffi_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
set(_ffi_specs_root "${CMAKE_CURRENT_SOURCE_DIR}/../../stdffi/mulib")
file(GLOB _ffi_default_specs CONFIGURE_DEPENDS "${_ffi_specs_root}/*.ffi")
set(_ffi_specs ${_ffi_default_specs})
set(ON1X_STDFFI_SPEC_FILES "" CACHE STRING "Override the standard FFI recipe input files")
if(ON1X_STDFFI_SPEC_FILES)
  set(_ffi_specs ${ON1X_STDFFI_SPEC_FILES})
endif()
set(_ffi_include "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")
set(_ffi_grammar "${CMAKE_CURRENT_SOURCE_DIR}/FFIHandler.llsynth")
set(_ffi_review "${CMAKE_CURRENT_SOURCE_DIR}/reviewed.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ffi_review}")
file(READ "${_ffi_review}" _ffi_review_json)
string(JSON _ffi_review_count LENGTH "${_ffi_review_json}")
if(_ffi_review_count LESS 1)
  message(FATAL_ERROR "FFIHandler requires a nonempty review lock")
endif()
math(EXPR _ffi_review_last "${_ffi_review_count} - 1")
foreach(_index RANGE ${_ffi_review_last})
  string(JSON _ffi_kernel MEMBER "${_ffi_review_json}" "${_index}")
  string(JSON _ffi_header GET "${_ffi_review_json}" "${_ffi_kernel}" header)
  if(NOT _ffi_kernel MATCHES "^[a-z][a-z0-9]*(-[a-z0-9]+)*$" OR
     NOT _ffi_header STREQUAL "mulib-${_ffi_kernel}.h")
    message(FATAL_ERROR "FFIHandler has a noncanonical reviewed header for ${_ffi_kernel}")
  endif()
  list(APPEND _ffi_headers "${_ffi_include}/${_ffi_header}")
endforeach()
set(_ffi_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_ffi_generated}")
set(_ffi_tables "${_ffi_generated}/on1x_stdffi_tables.hpp")
set(_ffi_bootstrap "${CMAKE_CURRENT_SOURCE_DIR}/generated/bootstrap.hpp")
set(_ffi_inputs "${_ffi_generated}/inputs.txt")
set(_ffi_compile "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/compile_recipes.py"
  --grammar "${_ffi_grammar}" --review "${_ffi_review}" --include "${_ffi_include}")
if(PERL_FOUND AND EXISTS "${_ffi_driver}")
  set(_ffi_parser "${_ffi_generated}/FFIHandler.py")
  set(_ffi_mode --parser "${_ffi_parser}")
  set(_ffi_input_asset "${_ffi_parser}")
  get_filename_component(_ffi_scripts "${_ffi_driver}" DIRECTORY)
  file(GLOB _ffi_generator_sources CONFIGURE_DEPENDS "${_ffi_scripts}/*.pl" "${_ffi_scripts}/*.pm")
  add_custom_command(OUTPUT "${_ffi_parser}"
    COMMAND "${PERL_EXECUTABLE}" "${_ffi_driver}" --mode=interpret -T python3
      -o "${_ffi_parser}" "${_ffi_grammar}"
    DEPENDS "${_ffi_grammar}" "${PERL_EXECUTABLE}" "${_ffi_driver}" "${_ffi_inputs}" ${_ffi_generator_sources}
    COMMENT "Generate the FFIHandler parser using LibStone LLSynth" VERBATIM)
  add_custom_target(on1x_stdffi_regenerate
    COMMAND ${_ffi_compile} ${_ffi_mode} --output "${_ffi_bootstrap}" ${_ffi_default_specs}
    DEPENDS "${_ffi_parser}" VERBATIM)
else()
  set(_ffi_mode --bootstrap "${_ffi_bootstrap}")
  set(_ffi_input_asset "${_ffi_bootstrap}")
endif()
file(GENERATE OUTPUT "${_ffi_inputs}"
  CONTENT "${_ffi_mode}\n${_ffi_specs}\n${_ffi_headers}\n${_ffi_driver}\n${_ffi_generator_sources}\n")
add_custom_command(OUTPUT "${_ffi_tables}"
  COMMAND ${_ffi_compile} ${_ffi_mode} --output "${_ffi_tables}" ${_ffi_specs}
  DEPENDS "${_ffi_input_asset}" "${_ffi_inputs}" "${_ffi_grammar}" "${_ffi_review}"
    compile_recipes.py ${_ffi_headers} ${_ffi_specs}
  COMMENT "Validate reviewed standard FFI recipes and compile immutable tables" VERBATIM)
add_custom_target(on1x_stdffi_tables DEPENDS "${_ffi_tables}")
add_dependencies(on1x_frontend on1x_stdffi_tables)
target_sources(on1x_frontend PRIVATE stdffi.cpp)
target_include_directories(on1x_frontend BEFORE PRIVATE "${_ffi_generated}")

if(BUILD_TESTING)
  if(_ffi_parser)
    add_test(NAME on1x.stdffi.recipe_compiler
      COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler.py"
        "${CMAKE_CURRENT_SOURCE_DIR}" "${_ffi_parser}" "${_ffi_include}"
        "${_ffi_specs_root}/clamp.ffi" "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(on1x.stdffi.recipe_compiler PROPERTIES TIMEOUT 60)
  endif()
  add_test(NAME on1x.stdffi.bootstrap
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/tests/bootstrap.py"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${_ffi_include}" "${_ffi_specs_root}/clamp.ffi"
      "${CMAKE_CURRENT_BINARY_DIR}" ${_ffi_parser})
  add_executable(on1x_stdffi_recipe_tests tests/recipes.cpp)
  target_include_directories(on1x_stdffi_recipe_tests PRIVATE "${_ffi_include}")
  target_link_libraries(on1x_stdffi_recipe_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.stdffi.checked_recipes COMMAND on1x_stdffi_recipe_tests)
  set_tests_properties(on1x.stdffi.bootstrap on1x.stdffi.checked_recipes PROPERTIES TIMEOUT 60)
endif()
