# The build-time parser is genuine LLSynth output; the frontend only links
# immutable C++ data. No rule interpreter is shipped with emitted programs.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Perl QUIET)
set(_meta_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_meta_generated}")
set(_meta_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
file(GLOB _meta_generator_sources CONFIGURE_DEPENDS
  "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pl"
  "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pm")
set(_meta_parser "${_meta_generated}/On1xMETA.py")
set(_meta_perl_options)
if(PERL_FOUND)
  set(_meta_perl_options --perl "${PERL_EXECUTABLE}")
endif()
if(PERL_FOUND AND EXISTS "${_meta_driver}")
  add_custom_command(OUTPUT "${_meta_parser}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/generate_parser.py"
      ${_meta_perl_options} --llsynth "${_meta_driver}"
      --grammar "${CMAKE_CURRENT_SOURCE_DIR}/On1xMETA.llsynth" --output "${_meta_parser}"
    DEPENDS On1xMETA.llsynth generate_parser.py generate.py ${_meta_generator_sources}
    COMMENT "Generate the On1xMETA LR/DFA parser with LibStone LLSynth" VERBATIM)
else()
  add_custom_command(OUTPUT "${_meta_parser}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/snapshot.py"
      --parser-only --libstone "${ON1X_LIBSTONE_ROOT}" --work "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/generated/On1xMETA.py" "${_meta_parser}"
    DEPENDS On1xMETA.llsynth generated/On1xMETA.py generated/parser_manifest.json
      snapshot.py generate_parser.py generate.py ${_meta_generator_sources}
    COMMENT "Verify the pinned LLSynth-generated On1xMETA bootstrap parser" VERBATIM)
endif()
set(_meta_default_rules
  "${CMAKE_CURRENT_SOURCE_DIR}/canonicalize.o1m"
  "${CMAKE_CURRENT_SOURCE_DIR}/desugar.o1m"
  "${CMAKE_CURRENT_SOURCE_DIR}/semantic.o1m")
set(_meta_legacy "${CMAKE_CURRENT_SOURCE_DIR}/../../processor")
get_filename_component(_meta_legacy "${_meta_legacy}" ABSOLUTE)
get_filename_component(_meta_previous "${CMAKE_CURRENT_SOURCE_DIR}/../processor" ABSOLUTE)
if("${ON1X_META_RULE_FILES}" STREQUAL
    "${_meta_previous}/canonicalize.o1m;${_meta_previous}/desugar.o1m;${_meta_previous}/semantic.o1m" OR
    "${ON1X_META_RULE_FILES}" STREQUAL
    "${_meta_legacy}/canonicalize.o1m;${_meta_legacy}/desugar.o1m;${_meta_legacy}/semantic.o1m")
  # Upgrade only the exact former default, not any authored rule override.
  set(ON1X_META_RULE_FILES "${_meta_default_rules}" CACHE STRING "On1xMETA rule inputs compiled into the frontend" FORCE)
endif()
set(ON1X_META_RULE_FILES "${_meta_default_rules}" CACHE STRING "On1xMETA rule inputs compiled into the frontend")
set(_meta_header "${_meta_generated}/on1x_meta_rules.hpp")
add_custom_command(
  OUTPUT "${_meta_header}" "${_meta_generated}/rules_manifest.json"
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/generate.py"
    --parser "${_meta_parser}" --output "${_meta_header}"
    --manifest "${_meta_generated}/rules_manifest.json" ${ON1X_META_RULE_FILES}
  DEPENDS "${_meta_parser}" generate.py ${ON1X_META_RULE_FILES}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/on1x/meta_schema.hpp"
  COMMENT "Validate and compile On1xMETA rewrite payloads and semantic constraints" VERBATIM)
add_custom_target(on1x_meta_tables DEPENDS "${_meta_header}" "${_meta_generated}/rules_manifest.json")
add_library(on1x_meta_generated INTERFACE)
add_dependencies(on1x_meta_generated on1x_meta_tables)
target_include_directories(on1x_meta_generated INTERFACE "${_meta_generated}")
if(TARGET on1x_frontend)
  add_dependencies(on1x_frontend on1x_meta_tables)
  target_include_directories(on1x_frontend BEFORE PRIVATE "${_meta_generated}")
endif()
if(BUILD_TESTING AND TARGET on1x_frontend AND NOT CMAKE_CROSSCOMPILING)
  if("${ON1X_META_RULE_FILES}" STREQUAL "${_meta_default_rules}")
    add_executable(on1x_test_analysis_directives tests/analysis_directives.cpp)
    target_link_libraries(on1x_test_analysis_directives PRIVATE on1x_frontend)
    add_test(NAME on1x.hir.analysis_directives COMMAND on1x_test_analysis_directives)
    add_test(NAME on1x.hir.analysis_directives_cli
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/analysis_directives_cli.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" "${PROJECT_SOURCE_DIR}/runtime"
        "${CMAKE_CURRENT_BINARY_DIR}" ${ON1X_TEST_C_FLAGS})
  endif()
  add_test(NAME on1x.processor.include_precedence
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_include_precedence.py"
      --processor "${CMAKE_CURRENT_SOURCE_DIR}"
      --work "${CMAKE_CURRENT_BINARY_DIR}/tests/include-precedence"
      --libstone "${ON1X_LIBSTONE_ROOT}" --cmake "${CMAKE_COMMAND}" --cxx "${CMAKE_CXX_COMPILER}"
      ${_meta_perl_options})
  set_tests_properties(on1x.processor.include_precedence PROPERTIES TIMEOUT 240)
  foreach(_variant IN ITEMS add subtract forbid)
    set(_variant_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/${_variant}")
    file(MAKE_DIRECTORY "${_variant_dir}")
    set(_variant_header "${_variant_dir}/on1x_meta_rules.hpp")
    add_custom_command(OUTPUT "${_variant_header}"
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/generate.py"
        --parser "${_meta_parser}" --output "${_variant_header}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${_variant}_zero.o1m"
      DEPENDS "${_meta_parser}" generate.py "tests/fixtures/${_variant}_zero.o1m"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../include/on1x/meta_schema.hpp"
      VERBATIM)
    # Compile both table consumers against the same variant to avoid mixing
    # different inline generated definitions in one executable.
    add_executable(on1x_meta_rule_${_variant}
      tests/rule_control.cpp processor.cpp processor_semantic.cpp
      "${_variant_header}")
    target_include_directories(on1x_meta_rule_${_variant} BEFORE PRIVATE "${_variant_dir}")
    target_link_libraries(on1x_meta_rule_${_variant} PRIVATE on1x_frontend)
    add_test(NAME on1x.processor.rule_${_variant}
      COMMAND on1x_meta_rule_${_variant} "${_variant}")
  endforeach()
endif()
add_custom_target(on1x_meta_regenerate
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/snapshot.py"
    --libstone "${ON1X_LIBSTONE_ROOT}" --work "${CMAKE_CURRENT_BINARY_DIR}" ${_meta_perl_options}
  COMMENT "Regenerate checked On1xMETA parser and rule snapshots" VERBATIM)
add_custom_target(on1x_meta_check
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/snapshot.py"
    --check --libstone "${ON1X_LIBSTONE_ROOT}" --work "${CMAKE_CURRENT_BINARY_DIR}" ${_meta_perl_options}
  COMMENT "Check deterministic LLSynth parser and rule snapshots" VERBATIM)
if(BUILD_TESTING AND PERL_FOUND AND EXISTS "${_meta_driver}")
  add_test(NAME on1x.processor.generator
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generator.py"
      --root "${CMAKE_CURRENT_SOURCE_DIR}" --work "${CMAKE_CURRENT_BINARY_DIR}/tests/generator"
      --cxx "${CMAKE_CXX_COMPILER}" --perl "${PERL_EXECUTABLE}" --llsynth "${_meta_driver}")
  set_tests_properties(on1x.processor.generator PROPERTIES TIMEOUT 180)
  add_test(NAME on1x.processor.snapshots
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/snapshot.py"
      --check --libstone "${ON1X_LIBSTONE_ROOT}" --work "${CMAKE_CURRENT_BINARY_DIR}/tests" ${_meta_perl_options})
  set_tests_properties(on1x.processor.snapshots PROPERTIES TIMEOUT 180)
endif()
