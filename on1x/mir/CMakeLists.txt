find_package(Perl QUIET)
set(_mir_grammar "${CMAKE_CURRENT_SOURCE_DIR}/MIR.llsynth")
set(_mir_source "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/core.mirp")
set(_mir_compiler "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/compile.py")
set(_mir_bootstrap "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/bootstrap.hpp")
set(_mir_driver "${ON1X_LIBSTONE_ROOT}/cornerstone/LLSynth/scripts/llsynth.pl")
set(_mir_database "${CMAKE_CURRENT_BINARY_DIR}/mir_policy_db.hpp")
set(_mir_compile "${Python3_EXECUTABLE}" -B "${_mir_compiler}"
  --grammar "${_mir_grammar}" --input "${_mir_source}")

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
  add_custom_command(OUTPUT "${_mir_database}"
    COMMAND ${_mir_compile} --parser "${_mir_parser}" --output "${_mir_database}"
    DEPENDS "${_mir_parser}" "${_mir_grammar}" "${_mir_source}" "${_mir_compiler}"
    COMMENT "Compile and validate the MIR policy database" VERBATIM)
  add_custom_target(on1x_mir_policy_regenerate
    COMMAND ${_mir_compile} --parser "${_mir_parser}" --output "${_mir_bootstrap}"
    DEPENDS "${_mir_parser}" VERBATIM)
else()
  add_custom_command(OUTPUT "${_mir_database}"
    COMMAND ${_mir_compile} --bootstrap "${_mir_bootstrap}" --output "${_mir_database}"
    DEPENDS "${_mir_grammar}" "${_mir_source}" "${_mir_compiler}" "${_mir_bootstrap}"
    COMMENT "Check the generated MIR policy bootstrap database" VERBATIM)
endif()

add_custom_target(on1x_mir_policy_tables DEPENDS "${_mir_database}")
add_dependencies(on1x_frontend on1x_mir_policy_tables)
target_sources(on1x_frontend PRIVATE specialize.cpp)
target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(BUILD_TESTING)
  add_executable(on1x_mir_specialize_tests rewrite/test_specialize.cpp)
  target_link_libraries(on1x_mir_specialize_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.mir.specialize COMMAND on1x_mir_specialize_tests)
  if(_mir_parser)
    add_test(NAME on1x.mir.policy_compiler
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rewrite/test_compiler.py"
        "${_mir_compiler}" "${_mir_grammar}" "${_mir_source}" "${_mir_parser}" "${_mir_bootstrap}")
    if(NOT CMAKE_CROSSCOMPILING)
      add_test(NAME on1x.mir.wire_grammar
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_wire.py"
          "${_mir_parser}" "$<TARGET_FILE:on1xc>")
    endif()
  endif()
endif()
