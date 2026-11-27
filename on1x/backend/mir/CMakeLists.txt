find_package(Perl QUIET)
set(_mir_grammar "${CMAKE_CURRENT_SOURCE_DIR}/MIR.llsynth")
set(_mir_source "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/core.mirp")
file(GLOB _mir_banks CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/banks/*.mirp")
set(_mir_compiler "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/compile.py")
set(_mir_bootstrap "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/bootstrap.hpp")
set(_mir_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
set(_mir_database "${CMAKE_CURRENT_BINARY_DIR}/mir_policy_db.hpp")
set(_mir_compile "${Python3_EXECUTABLE}" -B "${_mir_compiler}"
  --grammar "${_mir_grammar}" --input "${_mir_source}")
foreach(_mir_bank IN LISTS _mir_banks)
  list(APPEND _mir_compile --bank "${_mir_bank}")
endforeach()
set(_mir_sources "${_mir_grammar}" "${_mir_source}" "${_mir_compiler}" ${_mir_banks})

if(PERL_FOUND AND EXISTS "${_mir_driver}")
  set(_mir_parser "${CMAKE_CURRENT_BINARY_DIR}/mir_policy_parser.py")
  file(GLOB _mir_generator_deps CONFIGURE_DEPENDS
    "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pl"
    "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/*.pm")
  add_custom_command(OUTPUT "${_mir_parser}"
    COMMAND "${PERL_EXECUTABLE}" "${_mir_driver}" --mode=interpret -T python3
      -o "${_mir_parser}" "${_mir_grammar}"
    DEPENDS "${_mir_grammar}" "${_mir_driver}" ${_mir_generator_deps}
    COMMENT "Generate the MIR wire and policy parser with LibStone LLSynth" VERBATIM)
  add_custom_target(on1x_mir_policy_parser DEPENDS "${_mir_parser}")
  add_custom_command(OUTPUT "${_mir_database}"
    COMMAND ${_mir_compile} --parser "${_mir_parser}" --output "${_mir_database}"
    DEPENDS "${_mir_parser}" ${_mir_sources}
    COMMENT "Compile and validate the MIR policy database" VERBATIM)
  add_custom_target(on1x_mir_policy_regenerate
    COMMAND ${_mir_compile} --parser "${_mir_parser}" --output "${_mir_bootstrap}"
    DEPENDS "${_mir_parser}" ${_mir_sources} VERBATIM)
  add_dependencies(on1x_mir_policy_regenerate on1x_mir_policy_parser)
else()
  add_custom_command(OUTPUT "${_mir_database}"
    COMMAND ${_mir_compile} --bootstrap "${_mir_bootstrap}" --output "${_mir_database}"
    DEPENDS ${_mir_sources} "${_mir_bootstrap}"
    COMMENT "Check the generated MIR policy bootstrap database" VERBATIM)
endif()

set(_mir_tables "${_mir_database}")
foreach(_mir_profile IN ITEMS memgraph meta)
  if(_mir_profile STREQUAL "memgraph")
    set(_mir_action memgraph-local)
  else()
    set(_mir_action meta-canonical)
  endif()
  set(_mir_dir "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/${_mir_profile}")
  set(_mir_profile_source "${_mir_dir}/${_mir_action}.mirp")
  set(_mir_profile_bootstrap "${_mir_dir}/bootstrap.hpp")
  set(_mir_profile_database "${CMAKE_CURRENT_BINARY_DIR}/mir_policy_${_mir_profile}.hpp")
  set(_mir_profile_compile "${Python3_EXECUTABLE}" -B "${_mir_compiler}"
    --grammar "${_mir_grammar}" --profile "${_mir_action}" --input "${_mir_profile_source}")
  set(_mir_profile_deps "${_mir_profile_source}" "${_mir_grammar}" "${_mir_compiler}")
  if(_mir_parser)
    add_custom_command(OUTPUT "${_mir_profile_database}"
      COMMAND ${_mir_profile_compile} --parser "${_mir_parser}" --output "${_mir_profile_database}"
      DEPENDS "${_mir_parser}" ${_mir_profile_deps}
      COMMENT "Compile and validate the ${_mir_action} MIR policy" VERBATIM)
    add_custom_target(on1x_mir_policy_regenerate_${_mir_profile}
      COMMAND ${_mir_profile_compile} --parser "${_mir_parser}" --output "${_mir_profile_bootstrap}"
      DEPENDS "${_mir_parser}" ${_mir_profile_deps} VERBATIM)
    add_dependencies(on1x_mir_policy_regenerate_${_mir_profile} on1x_mir_policy_parser)
    add_dependencies(on1x_mir_policy_regenerate on1x_mir_policy_regenerate_${_mir_profile})
  else()
    add_custom_command(OUTPUT "${_mir_profile_database}"
      COMMAND ${_mir_profile_compile} --bootstrap "${_mir_profile_bootstrap}" --output "${_mir_profile_database}"
      DEPENDS "${_mir_profile_bootstrap}" ${_mir_profile_deps}
      COMMENT "Check the ${_mir_action} MIR policy bootstrap database" VERBATIM)
  endif()
  list(APPEND _mir_tables "${_mir_profile_database}")
  if(BUILD_TESTING)
    # Each bank can add its own focused runtime and compiler tests without
    # editing a shared registry or test file.
    file(GLOB _mir_profile_test CONFIGURE_DEPENDS "${_mir_dir}/test_specialize.cpp")
    if(_mir_profile_test)
      add_executable(on1x_mir_${_mir_profile}_tests "${_mir_profile_test}")
      target_link_libraries(on1x_mir_${_mir_profile}_tests PRIVATE on1x_frontend)
      add_test(NAME on1x.mir.specialize_${_mir_profile} COMMAND on1x_mir_${_mir_profile}_tests)
    endif()
    if(_mir_parser)
      add_test(NAME on1x.mir.policy_${_mir_profile}
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/test_profile.py"
          ${_mir_profile_compile} "${_mir_parser}" "${_mir_profile_bootstrap}")
    endif()
  endif()
endforeach()

add_custom_target(on1x_mir_policy_tables DEPENDS ${_mir_tables})
if(_mir_parser)
  add_dependencies(on1x_mir_policy_tables on1x_mir_policy_parser)
endif()
add_dependencies(on1x_frontend on1x_mir_policy_tables)
target_sources(on1x_frontend PRIVATE specialize.cpp)
target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(BUILD_TESTING)
  add_executable(on1x_mir_specialize_tests rewrite/test_specialize.cpp)
  target_link_libraries(on1x_mir_specialize_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.mir.specialize COMMAND on1x_mir_specialize_tests)
  if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME on1x.mir.scoped_dispatch
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/test_dispatch.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
  if(_mir_parser)
    add_test(NAME on1x.mir.policy_compiler
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/test_compiler.py"
        "${_mir_compiler}" "${_mir_grammar}" "${_mir_source}" "${_mir_parser}" "${_mir_bootstrap}" ${_mir_banks})
    if(NOT CMAKE_CROSSCOMPILING)
      add_test(NAME on1x.mir.wire_grammar
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_wire.py"
          "${_mir_parser}" "$<TARGET_FILE:on1xc>")
    endif()
  endif()
endif()
