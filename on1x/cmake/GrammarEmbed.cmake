# A native On1x processor with a build-host-only liblr table-generator companion.
option(ON1X_BUILD_GRAMMAREMBED "Build the native GrammarEmbed PPX (requires Firestone liblr)" ON)
if(ON1X_BUILD_GRAMMAREMBED)
  if(NOT EXISTS "${ON1X_LIBSTONE_ROOT}/firestone/liblr/ParserGenerator.hpp")
    message(FATAL_ERROR "GrammarEmbed requires ON1X_LIBSTONE_ROOT/firestone/liblr; set ON1X_BUILD_GRAMMAREMBED=OFF for a bootstrap without it")
  endif()
  set(_grammar_dir "${CMAKE_CURRENT_BINARY_DIR}/grammarembed")
  file(MAKE_DIRECTORY "${_grammar_dir}")
  add_custom_command(OUTPUT "${_grammar_dir}/machinery.sexpr"
    COMMAND "$<TARGET_FILE:on1xc>" --no-prelude --emit-ast
      "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/grammarembed/machinery.on1x"
      -o "${_grammar_dir}/machinery.sexpr"
    DEPENDS on1xc stdppx/grammarembed/machinery.on1x VERBATIM)
  add_custom_command(OUTPUT "${_grammar_dir}/grammar_resources.hpp"
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/grammarembed/generate.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/grammarembed/catalog.json"
      "${_grammar_dir}/machinery.sexpr" "${_grammar_dir}/grammar_resources.hpp"
    DEPENDS stdppx/grammarembed/generate.py stdppx/grammarembed/catalog.json
      "${_grammar_dir}/machinery.sexpr" VERBATIM)
  add_library(on1x_grammarembed_bridge STATIC stdppx/grammarembed/bridge.cpp
    "${_grammar_dir}/grammar_resources.hpp")
  target_include_directories(on1x_grammarembed_bridge PRIVATE "${_grammar_dir}")
  target_include_directories(on1x_grammarembed_bridge SYSTEM PRIVATE "${ON1X_LIBSTONE_ROOT}/firestone")
  list(APPEND _native_processors GrammarEmbed)
endif()
