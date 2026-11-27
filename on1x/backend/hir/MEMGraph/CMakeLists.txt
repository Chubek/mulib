# MEMGraph rules are compiled at build time; only immutable tables reach C++.
# Host lexical planning is independent of generated rules.
target_sources(on1x_frontend PRIVATE memgraph.cpp lexical_slots.cpp resources.cpp storage.cpp)
if(BUILD_TESTING)
  add_executable(on1x_memgraph_lexical_slots_tests tests/test_lexical_slots.cpp)
  target_link_libraries(on1x_memgraph_lexical_slots_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.memgraph.lexical_slots COMMAND on1x_memgraph_lexical_slots_tests)
  set_tests_properties(on1x.memgraph.lexical_slots PROPERTIES TIMEOUT 60)
  if(NOT CMAKE_CROSSCOMPILING)
    set(_resource_test_flags)
    foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
      list(APPEND _resource_test_flags "--cflag=${_flag}")
    endforeach()
    add_test(NAME on1x.memgraph.resources
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resources.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        "${CMAKE_CURRENT_BINARY_DIR}/tests/resources" "${CMAKE_C_COMPILER}" ${_resource_test_flags})
    set_tests_properties(on1x.memgraph.resources PROPERTIES TIMEOUT 120)
  endif()
endif()
find_package(Perl QUIET)
set(_mmg_driver "")
foreach(_mmg_root IN ITEMS "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth" "${ON1X_LIBSTONE_ROOT}/LLSynth")
  set(_mmg_candidate "${_mmg_root}/scripts/llsynth.pl")
  if(EXISTS "${_mmg_candidate}" AND NOT IS_DIRECTORY "${_mmg_candidate}")
    set(_mmg_driver "${_mmg_candidate}")
    break()
  endif()
endforeach()

set(_mmg_default OFF)
if(PERL_FOUND AND _mmg_driver)
  # FindPerl can accept a stale cache entry. Probe the configured driver and
  # its Perl dependencies rather than enabling tables for an unusable tool.
  execute_process(COMMAND "${PERL_EXECUTABLE}" "${_mmg_driver}" --version
    RESULT_VARIABLE _mmg_driver_result OUTPUT_VARIABLE _mmg_driver_version
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET TIMEOUT 10)
  if(_mmg_driver_result STREQUAL "0" AND _mmg_driver_version MATCHES "^llsynth[.]pl [0-9]")
    set(_mmg_default ON)
  endif()
endif()
option(ON1X_WITH_MEMGRAPH "Build On1x MEMGraph using the LibStone LLSynth generator" ${_mmg_default})
if(NOT ON1X_WITH_MEMGRAPH)
  if(_mmg_default)
    message(STATUS "On1x MEMGraph: disabled (ON1X_WITH_MEMGRAPH=OFF)")
  else()
    message(STATUS "On1x MEMGraph: disabled (requires Perl and LibStone LLSynth under ON1X_LIBSTONE_ROOT)")
  endif()
  return()
endif()
if(NOT _mmg_default)
  message(FATAL_ERROR "ON1X_WITH_MEMGRAPH=ON requires Perl and the real llsynth.pl under ON1X_LIBSTONE_ROOT (cornerstone/LLSynth/scripts or LLSynth/scripts)")
endif()
message(STATUS "On1x MEMGraph: enabled (LibStone LLSynth: ${_mmg_driver})")

get_filename_component(_mmg_driver_real "${_mmg_driver}" REALPATH)
get_filename_component(_mmg_scripts "${_mmg_driver_real}" DIRECTORY)
file(GLOB _mmg_generator_sources CONFIGURE_DEPENDS "${_mmg_scripts}/*.pl" "${_mmg_scripts}/*.pm")
file(GLOB _mmg_rules CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.mmg" "${CMAKE_CURRENT_SOURCE_DIR}/*.mmgi")
set(_mmg_grammar "${CMAKE_CURRENT_SOURCE_DIR}/MEMGraph.llsynth")
set(_mmg_compiler "${CMAKE_CURRENT_SOURCE_DIR}/tools/mmgc.py")
set(_mmg_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_mmg_parser "${_mmg_generated}/memgraph_parser.py")
set(_mmg_header "${_mmg_generated}/on1x_memgraph_rules.hpp")
set(_mmg_manifest "${_mmg_generated}/MEMGraph.manifest.json")
file(MAKE_DIRECTORY "${_mmg_generated}")
# A removed source is also a change, even when none of the remaining files
# has a newer timestamp than the tables.
set(_mmg_sources "${_mmg_generated}/sources.txt")
file(GENERATE OUTPUT "${_mmg_sources}" CONTENT "${_mmg_rules}\n")
set(_mmg_toolchain "${_mmg_generated}/toolchain.txt")
file(GENERATE OUTPUT "${_mmg_toolchain}"
  CONTENT "${PERL_EXECUTABLE}\n${_mmg_driver}\n${_mmg_driver_real}\n${_mmg_generator_sources}\n")
add_custom_command(OUTPUT "${_mmg_parser}"
  COMMAND "${PERL_EXECUTABLE}" "${_mmg_driver}" --mode=interpret -T python3
    -o "${_mmg_parser}" "${_mmg_grammar}"
  DEPENDS "${_mmg_grammar}" "${_mmg_driver}" "${PERL_EXECUTABLE}"
    "${_mmg_toolchain}" ${_mmg_generator_sources}
  COMMENT "Generate the MEMGraph parser with LibStone LLSynth" VERBATIM)
add_custom_command(OUTPUT "${_mmg_header}" "${_mmg_manifest}"
  COMMAND "${Python3_EXECUTABLE}" -IB "${_mmg_compiler}" --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --parser "${_mmg_parser}" --header "${_mmg_header}" --manifest "${_mmg_manifest}"
  DEPENDS "${_mmg_parser}" "${_mmg_compiler}" "${_mmg_sources}" ${_mmg_rules}
  COMMENT "Validate MEMGraph rules and compile the fixed-array profile" VERBATIM)
add_custom_target(on1x_memgraph_tables DEPENDS "${_mmg_header}" "${_mmg_manifest}")
add_dependencies(on1x_frontend on1x_memgraph_tables)
target_include_directories(on1x_frontend BEFORE PRIVATE "${_mmg_generated}")
target_compile_definitions(on1x_frontend PRIVATE ON1X_WITH_MEMGRAPH=1)

if(BUILD_TESTING)
  add_test(NAME on1x.memgraph.mmgc
    COMMAND "${CMAKE_COMMAND}" -E env "ON1X_MEMGRAPH_TEST_PARSER=${_mmg_parser}"
      "PYTHONDONTWRITEBYTECODE=1" "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mmgc.py")
  add_executable(on1x_memgraph_hir_tests ../../tests/test_memgraph_hir.cpp)
  target_link_libraries(on1x_memgraph_hir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.memgraph.hir COMMAND on1x_memgraph_hir_tests)
  set_tests_properties(on1x.memgraph.mmgc on1x.memgraph.hir PROPERTIES TIMEOUT 60)
  if(NOT CMAKE_CROSSCOMPILING)
    set(_mmg_test_flags)
    foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
      list(APPEND _mmg_test_flags "--cflag=${_flag}")
    endforeach()
    if(ON1X_HAS_FIRESTONE_REGEX)
      list(APPEND _mmg_test_flags --regex)
    endif()
    add_test(NAME on1x.memgraph.c11
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        "${CMAKE_CURRENT_BINARY_DIR}/tests/c11" "${CMAKE_C_COMPILER}" ${_mmg_test_flags})
    set_tests_properties(on1x.memgraph.c11 PROPERTIES TIMEOUT 120)
  endif()
endif()
