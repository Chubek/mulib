cmake_minimum_required(VERSION 3.20)
project(On1x VERSION 0.1.0 LANGUAGES C CXX)
include(CTest)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(_libstone_default "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libstone")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libstone/scripts/asdl.pl")
  set(_libstone_default "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libstone")
endif()

if(DEFINED ENV{ON1X_LIBSTONE_ROOT} AND NOT "$ENV{ON1X_LIBSTONE_ROOT}" STREQUAL "")
  set(_libstone_default "$ENV{ON1X_LIBSTONE_ROOT}")
endif()

set(ON1X_LIBSTONE_ROOT "${_libstone_default}" CACHE PATH "LibStone source checkout")
option(ON1X_REQUIRE_LIBSTONE "Fail if LibStone generators are unavailable" OFF)
option(ON1X_USE_FALLBACK_SCANNER "Use the bootstrap scanner even when LibStone is present" OFF)
option(ON1X_WITH_EXOML "Build optional ExoML contract-verification tool" OFF)
option(ON1X_WITH_EXOML_EXTRACT "Enable real libclang CDecl extraction in on1x-bind" OFF)
option(ON1X_WITH_EXOTRACT "Build MuLIB static binding discovery and generation" OFF)
option(ON1X_WITH_EXOTRACT_LUA "Build Exotract's Lua ExoLib frontend for KernelBind discovery" OFF)
option(ON1X_BUILD_EXTENSION_SDK "Build and install the On1x extension SDK" ON)
option(ON1X_BUILD_DYNAMIC_SDK_PLUGINS "Build the Linux x86-64 isolated SDK plugin worker" ON)
set(ON1X_LINKED_SDK_EXTENSION "" CACHE FILEPATH
  "Optional trusted C/C++ source defining on1x_sdk_linked_extension for on1xc")
if(ON1X_BUILD_EXTENSION_SDK)
  add_subdirectory(sdk)
elseif(ON1X_LINKED_SDK_EXTENSION)
  message(FATAL_ERROR "ON1X_LINKED_SDK_EXTENSION requires ON1X_BUILD_EXTENSION_SDK")
endif()
if(ON1X_WITH_EXOTRACT_LUA AND NOT ON1X_WITH_EXOTRACT)
  message(FATAL_ERROR "ON1X_WITH_EXOTRACT_LUA requires ON1X_WITH_EXOTRACT")
endif()
if(ON1X_WITH_EXOTRACT AND NOT ON1X_WITH_EXOML_EXTRACT)
  message(FATAL_ERROR "ON1X_WITH_EXOTRACT requires ON1X_WITH_EXOML=ON and ON1X_WITH_EXOML_EXTRACT=ON")
endif()
option(ON1X_WITH_NOVOSTONE "Build optional MIR Datalog/CCDB analyses" OFF)
option(ON1X_WITH_CHISEL "Build optional resource memory checking" OFF)
option(ON1X_WITH_NEROTYPICAL "Build optional independent HIR structural type checking" OFF)
set(_on1x_syren_default OFF)
if(EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/syren/include/syren/Syren.hpp")
  set(_on1x_syren_default ON)
endif()
option(ON1X_WITH_SYREN "Build Syren source diagnostics and explicit repair tooling" ${_on1x_syren_default})
set(_on1x_wetstone_default OFF)
if(EXISTS "${ON1X_LIBSTONE_ROOT}/wetstone/verification/AnalysisSuite.hpp")
  set(_on1x_wetstone_default ON)
endif()
option(ON1X_WITH_WETSTONE "Run the bounded Wetstone static-analysis prelude" ${_on1x_wetstone_default})
option(ON1X_WITH_PROJECT_QMR "Enable the optional Moonstone QMR compiler for On1xfile templates when available" ON)
if(ON1X_WITH_EXOML_EXTRACT AND NOT ON1X_WITH_EXOML)
  message(FATAL_ERROR "ON1X_WITH_EXOML_EXTRACT requires ON1X_WITH_EXOML")
endif()
set(ON1X_TEST_C_FLAGS "" CACHE STRING "Extra generated-program compiler/linker options (semicolon-separated)")
if(ON1X_REQUIRE_LIBSTONE AND ON1X_USE_FALLBACK_SCANNER)
  message(FATAL_ERROR "ON1X_REQUIRE_LIBSTONE and ON1X_USE_FALLBACK_SCANNER are mutually exclusive")
endif()
set(_asdl "${ON1X_LIBSTONE_ROOT}/scripts/asdl.pl")
set(_aurocks "${ON1X_LIBSTONE_ROOT}/scripts/aurocks.pl")
if(NOT ON1X_USE_FALLBACK_SCANNER AND EXISTS "${_asdl}" AND EXISTS "${_aurocks}")
  find_package(Perl REQUIRED)
  set(_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${_generated}")
  add_custom_command(
    OUTPUT "${_generated}/on1x_ast.h" "${_generated}/on1x_ast.c" "${_generated}/on1x_parser.c" "${_generated}/ppx_ast.h" "${_generated}/ppx_ast.c"
    COMMAND "${CMAKE_COMMAND}" "-DPERL=${PERL_EXECUTABLE}" "-DSCRIPTS=${ON1X_LIBSTONE_ROOT}/scripts"
      "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT=${_generated}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateScanner.cmake"
    DEPENDS "${_asdl}" "${_aurocks}" grammar/On1x.asdl grammar/On1x.g grammar/PPX.asdl
      cmake/GenerateScanner.cmake VERBATIM)
  add_library(on1x_scanner STATIC "${_generated}/on1x_parser.c" "${_generated}/on1x_ast.c" "${_generated}/ppx_ast.c" src/ppx_asdl.c)
  target_include_directories(on1x_scanner PRIVATE "${_generated}" include)
  set(ON1X_SCANNER_DESCRIPTION "LibStone ASDL+Aurocks generated frontend")
  set(ON1X_GENERATED_FRONTEND TRUE)
elseif(ON1X_REQUIRE_LIBSTONE)
  message(FATAL_ERROR "Missing LibStone generators under ${ON1X_LIBSTONE_ROOT}/scripts")
else()
  add_library(on1x_scanner STATIC src/lexer_fallback.cpp)
  target_include_directories(on1x_scanner PRIVATE include)
  set(ON1X_SCANNER_DESCRIPTION "portable fallback scanner (bootstrap only)")
endif()
message(STATUS "On1x scanner: ${ON1X_SCANNER_DESCRIPTION}")
include(cmake/DependencyReport.cmake)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(ON1X_BUILD_EXTENSION_SDK AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME on1x.sdk.installed_consumer
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_package.py"
      "${CMAKE_COMMAND}" "${CMAKE_CURRENT_BINARY_DIR}" "$<CONFIG>"
      "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
      "${CMAKE_C_FLAGS}" "${CMAKE_CXX_FLAGS}" "${CMAKE_EXE_LINKER_FLAGS}")
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS grammar/PPX.schema.json)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/grammar/PPX.schema.json" ON1X_SCHEMA_TEXT)
configure_file(cmake/ppx_schema.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/ppx_schema.hpp" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS stdlib/diagnostics/catalog.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS tools/generate-diagnostic-catalog.py)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-diagnostic-catalog.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/diagnostics/catalog.json"
    "${CMAKE_CURRENT_BINARY_DIR}/diagnostic_catalog.hpp"
  COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS stdppx/category/features.sexpr)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/category/features.sexpr" ON1X_EXTENSION_REGISTRY)
configure_file(cmake/extensions_registry.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/extensions_registry.hpp" @ONLY)
add_library(on1x_frontend STATIC src/source.cpp src/lexer.cpp src/type.cpp src/nominal.cpp src/inherent.cpp src/cliche.cpp src/numeric.cpp src/parser.cpp src/loader.cpp src/discovery.cpp src/infer.cpp src/sema.cpp src/analysis.cpp src/diagnostic.cpp src/concurrency.cpp src/codegen.cpp
  src/sexpr.cpp src/ppx_schema.cpp src/ppx_ast.cpp src/macros.cpp src/macro_rules.cpp src/ppx.cpp src/primitives.cpp src/staging.cpp src/extensions.cpp src/extension_evidence.cpp src/extensions_hash.c src/canonical/control.cpp src/patterns.cpp src/canonical/values.cpp src/canonical/verify_values.cpp
  src/backend/mir.cpp src/backend/lower.cpp src/backend/serialize.cpp src/backend/evaluate.cpp src/backend/emit_c.cpp src/backend/passes.cpp)
target_include_directories(on1x_frontend PUBLIC include)
target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_sources(on1x_frontend PRIVATE src/regex.cpp)
if(EXISTS "${ON1X_LIBSTONE_ROOT}/firestone/liblr/ThompsonConstruction.hpp" AND
   EXISTS "${ON1X_LIBSTONE_ROOT}/firestone/liblr/UTF8Automaton.hpp")
  target_include_directories(on1x_frontend SYSTEM PRIVATE "${ON1X_LIBSTONE_ROOT}/firestone")
  target_compile_definitions(on1x_frontend PRIVATE ON1X_WITH_FIRESTONE_REGEX=1)
  set(ON1X_HAS_FIRESTONE_REGEX TRUE)
  message(STATUS "On1x core regex: LibStone Firestone compiler SDK")
else()
  message(STATUS "On1x core regex: unavailable without LibStone Firestone headers")
endif()
target_compile_definitions(on1x_frontend PRIVATE ON1X_STDLIB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/stdlib")
if(ON1X_WITH_WETSTONE)
  if(EXISTS "${ON1X_LIBSTONE_ROOT}/wetstone/verification/AnalysisSuite.hpp")
    target_compile_definitions(on1x_frontend PRIVATE ON1X_WITH_WETSTONE=1)
    target_include_directories(on1x_frontend SYSTEM PRIVATE "${ON1X_LIBSTONE_ROOT}")
    message(STATUS "On1x static analysis: Wetstone prelude enabled")
  else()
    message(WARNING "ON1X_WITH_WETSTONE requested, but Wetstone headers are unavailable; using the bounded no-analysis fallback")
  endif()
else()
  message(STATUS "On1x static analysis: Wetstone prelude unavailable/disabled")
endif()
if(ON1X_GENERATED_FRONTEND)
  target_compile_definitions(on1x_frontend PRIVATE ON1X_ASDL_PPX=1)
endif()
target_link_libraries(on1x_frontend PUBLIC on1x_scanner)
set(_pretty_impl "${CMAKE_CURRENT_BINARY_DIR}/on1x_pretty_impl.c")
add_custom_command(OUTPUT "${_pretty_impl}"
  COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/prettyprint/on1x/implementation.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${_pretty_impl}"
  DEPENDS stdlib/prettyprint/on1x/implementation.py ../MANIFEST.yaml
    stdlib/mulib/housekeeping.py stdlib/mulib/catalog.json tools/on1x-contracts.py
    contracts-exotract/catalog.json
  VERBATIM)
add_library(on1x_pretty_kernels STATIC "${_pretty_impl}")
target_include_directories(on1x_pretty_kernels PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(on1x_pretty_kernels PUBLIC ${CMAKE_DL_LIBS})
if(UNIX)
  target_link_libraries(on1x_pretty_kernels PUBLIC m)
endif()
add_executable(on1xc src/cli.cpp stdlib/prettyprint/on1x/formatter.cpp)
target_link_libraries(on1xc PRIVATE on1x_frontend on1x_pretty_kernels)
target_compile_definitions(on1xc PRIVATE ON1X_DRIVER_SOURCE_TOOLS="${CMAKE_CURRENT_SOURCE_DIR}/tools")
if(ON1X_BUILD_EXTENSION_SDK)
  add_subdirectory(backends)
  target_sources(on1xc PRIVATE sdk/src/Driver.cpp)
  target_include_directories(on1xc PRIVATE sdk/src)
  target_link_libraries(on1xc PRIVATE On1x::SDK on1x_builtin_backends)
  target_compile_definitions(on1xc PRIVATE
    ON1X_WITH_EXTENSION_SDK=1
    ON1X_DRIVER_SOURCE_SDK_TOOL="${CMAKE_CURRENT_SOURCE_DIR}/sdk/tools/on1x-sdk")
  if(ON1X_BUILD_DYNAMIC_SDK_PLUGINS AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
      CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_library(on1x_plugin_kernels STATIC sdk/src/PluginKernels.c)
    target_include_directories(on1x_plugin_kernels PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    add_executable(on1x-plugin-worker sdk/src/PluginWorker.cpp)
    target_link_libraries(on1x-plugin-worker PRIVATE on1x_plugin_kernels
      "-Wl,--whole-archive" On1x::SDK "-Wl,--no-whole-archive" ${CMAKE_DL_LIBS})
    set_target_properties(on1x-plugin-worker PROPERTIES ENABLE_EXPORTS ON)
    target_sources(on1xc PRIVATE sdk/src/PluginHost.cpp)
    target_link_libraries(on1xc PRIVATE on1x_plugin_kernels)
    target_compile_definitions(on1xc PRIVATE ON1X_WITH_DYNAMIC_SDK_PLUGINS=1)
    add_dependencies(on1xc on1x-plugin-worker)
    install(TARGETS on1x-plugin-worker RUNTIME DESTINATION bin)
    if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
      add_library(on1x_sdk_dynamic_fixture MODULE sdk/tests/linked_extension.c)
      target_include_directories(on1x_sdk_dynamic_fixture PRIVATE sdk/include)
      add_test(NAME on1x.sdk.dynamic_plugins
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_dynamic.py"
          "$<TARGET_FILE:on1xc>" "$<TARGET_FILE:on1x_sdk_dynamic_fixture>" "${CMAKE_C_COMPILER}")
    endif()
  endif()
endif()
if(ON1X_LINKED_SDK_EXTENSION)
  target_sources(on1xc PRIVATE "${ON1X_LINKED_SDK_EXTENSION}")
  target_compile_definitions(on1xc PRIVATE ON1X_WITH_LINKED_SDK_EXTENSION=1)
  if(BUILD_TESTING AND UNIX AND NOT CMAKE_CROSSCOMPILING AND
     ON1X_LINKED_SDK_EXTENSION STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/linked_extension.c")
    separate_arguments(_sdk_test_c_flags NATIVE_COMMAND "${CMAKE_C_FLAGS}")
    add_test(NAME on1x.sdk.linked_compiler
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_linked.py"
        "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" ${_sdk_test_c_flags})
  endif()
endif()
if(ON1X_WITH_NEROTYPICAL)
  include(cmake/Nerotypical.cmake)
endif()
if(ON1X_WITH_NOVOSTONE)
  include(cmake/Novostone.cmake)
endif()
add_library(on1x_runtime STATIC runtime/on1x_runtime.c)
target_include_directories(on1x_runtime PUBLIC runtime)
option(ON1X_BUILD_NATIVE_PPX "Build the self-hosted On1x PPX processors" ON)
target_compile_definitions(on1x_frontend PRIVATE ON1X_BUILD_NATIVE_PPX=$<BOOL:${ON1X_BUILD_NATIVE_PPX}>)
if(ON1X_BUILD_NATIVE_PPX)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Native build-host PPXs cannot be target executables; configure ON1X_BUILD_NATIVE_PPX=OFF for cross compilation")
  endif()
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stdppx")
  set(_ppx_manifest "(On1xfile 1 (kind \"ppx\") (name \"stdppx\") (protocol \"1\")\n")
  configure_file(grammar/PPX.schema.json "${CMAKE_CURRENT_BINARY_DIR}/stdppx/category.schema.json" COPYONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stdppx/category.schema.json" DESTINATION libexec/on1x)
  file(GLOB_RECURSE _native_stdlib CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/*.on1x")
  file(GLOB _native_ppx_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/native/*.on1x")
  set(_luaembed_dir "${CMAKE_CURRENT_BINARY_DIR}/luaembed")
  add_custom_command(
    OUTPUT "${_luaembed_dir}/luaembed_impl.c" "${_luaembed_dir}/luaembed_resources.h"
           "${_luaembed_dir}/luaembed_plan.json"
    COMMAND "${Python3_EXECUTABLE}" -IB "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/luaembed/generate.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/.." "${_luaembed_dir}"
    DEPENDS stdppx/luaembed/generate.py stdppx/luaembed/messages.json stdppx/luaembed/llembed.lua
      stdlib/mulib/housekeeping.py stdlib/mulib/catalog.json tools/on1x-contracts.py
      contracts-exotract/catalog.json ../MANIFEST.yaml
    VERBATIM)
  add_library(on1x_luaembed_kernels STATIC "${_luaembed_dir}/luaembed_impl.c")
  target_include_directories(on1x_luaembed_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
  find_package(Threads REQUIRED)
  target_link_libraries(on1x_luaembed_kernels PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
  if(UNIX)
    target_link_libraries(on1x_luaembed_kernels PUBLIC m)
  endif()
  add_library(on1x_luaembed_bridge STATIC stdppx/luaembed/runtime.c "${_luaembed_dir}/luaembed_resources.h")
  target_include_directories(on1x_luaembed_bridge PRIVATE "${_luaembed_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/../include")
  target_link_libraries(on1x_luaembed_bridge PUBLIC on1x_luaembed_kernels)
  set(_native_processors identity checks control unroll derive fold pipe compose assertions cond format logging state_machine tap trace protocols diagnostics LuaEmbed)
  include(cmake/GrammarEmbed.cmake)
  foreach(_processor IN LISTS _native_processors)
    string(APPEND _ppx_manifest " (processor \"${_processor}\" \"on1x-ppx-${_processor}\" \"category.schema.json\")\n")
    set(_native_input "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/native/${_processor}.on1x")
    set(_native_c "${CMAKE_CURRENT_BINARY_DIR}/stdppx/${_processor}.c")
    add_custom_command(OUTPUT "${_native_c}"
      COMMAND "$<TARGET_FILE:on1xc>" --emit-c "${_native_input}" -o "${_native_c}"
      DEPENDS on1xc ${_native_ppx_sources} ${_native_stdlib} VERBATIM)
    add_executable(on1x_ppx_${_processor} "${_native_c}")
    target_link_libraries(on1x_ppx_${_processor} PRIVATE on1x_runtime)
    if(_processor STREQUAL "LuaEmbed")
      target_link_libraries(on1x_ppx_${_processor} PRIVATE on1x_luaembed_bridge)
    endif()
    if(_processor STREQUAL "GrammarEmbed")
      target_link_libraries(on1x_ppx_${_processor} PRIVATE on1x_grammarembed_bridge)
    endif()
    set_target_properties(on1x_ppx_${_processor} PROPERTIES OUTPUT_NAME "on1x-ppx-${_processor}" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stdppx")
    install(TARGETS on1x_ppx_${_processor} RUNTIME DESTINATION libexec/on1x)
  endforeach()
  string(APPEND _ppx_manifest ")\n")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stdppx/On1xfile" "${_ppx_manifest}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stdppx/On1xfile" DESTINATION libexec/on1x)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/stdppx/on1x-ppx-protocols.pin"
    COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:on1x_ppx_protocols>"
      "-DSCHEMA=${CMAKE_CURRENT_BINARY_DIR}/stdppx/category.schema.json"
      "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/stdppx/on1x-ppx-protocols.pin" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PinExtension.cmake"
    DEPENDS on1x_ppx_protocols "${CMAKE_CURRENT_BINARY_DIR}/stdppx/category.schema.json" cmake/PinExtension.cmake VERBATIM)
  add_custom_target(on1x_category_pin ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdppx/on1x-ppx-protocols.pin")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stdppx/on1x-ppx-protocols.pin" DESTINATION libexec/on1x)
endif()

install(TARGETS on1xc RUNTIME DESTINATION bin)
install(DIRECTORY stdlib stdppx runtime grammar contracts-exoml contracts-exotract DESTINATION share/on1x
  PATTERN "__pycache__" EXCLUDE PATTERN "*.pyc" EXCLUDE)
install(PROGRAMS tools/on1x-build.py tools/on1x-project.py tools/on1x-mulib.py tools/on1x-exotract.py tools/on1x-contracts.py DESTINATION share/on1x/tools)
install(FILES tools/on1x_project.py DESTINATION share/on1x/tools)

if(ON1X_WITH_EXOML OR ON1X_WITH_CHISEL OR ON1X_WITH_SYREN)
  if(NOT EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/CMakeLists.txt")
    message(FATAL_ERROR "ExoML/Chisel/Syren requires a checkout at ON1X_LIBSTONE_ROOT")
  endif()
  set(MOONSTONE_BUILD_CHISEL ${ON1X_WITH_CHISEL} CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_EXOML ${ON1X_WITH_EXOML} CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_EXOTRACT ${ON1X_WITH_EXOTRACT} CACHE BOOL "" FORCE)
  if(ON1X_WITH_EXOTRACT)
    set(EXOTRACT_ENABLE_LUA ${ON1X_WITH_EXOTRACT_LUA} CACHE BOOL "" FORCE)
    set(EXOTRACT_ENABLE_LMDB OFF CACHE BOOL "" FORCE)
    set(EXOTRACT_ENABLE_UCPP OFF CACHE BOOL "" FORCE)
    set(EXOTRACT_ENABLE_CPARSER OFF CACHE BOOL "" FORCE)
    set(EXOTRACT_ENABLE_EXOML ON CACHE BOOL "" FORCE)
    set(EXOTRACT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  endif()
  set(MOONSTONE_BUILD_SYREN ${ON1X_WITH_SYREN} CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_SUPERSONIC OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_PLAID OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_PROLIFIC OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(MOONSTONE_EXOML_EXTRACT ${ON1X_WITH_EXOML_EXTRACT} CACHE BOOL "" FORCE)
  add_subdirectory("${ON1X_LIBSTONE_ROOT}/moonstone" "${CMAKE_CURRENT_BINARY_DIR}/libstone-moonstone" EXCLUDE_FROM_ALL)
  if(ON1X_WITH_EXOTRACT)
    add_custom_target(on1x-exotract ALL DEPENDS exotract)
  endif()
  if(ON1X_WITH_EXOTRACT_LUA AND NOT TARGET Moonstone::ExotractLua)
    message(FATAL_ERROR "KernelBind discovery requires LibStone's vendored Lua/Kaguya")
  endif()
endif()
if(ON1X_WITH_SYREN)
  if(NOT EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/syren/include/syren/Repair.hpp" OR
     NOT EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/syren/include/syren/Syren.hpp")
    message(FATAL_ERROR "Syren requires Syren.hpp and Repair.hpp under ON1X_LIBSTONE_ROOT/moonstone/syren")
  endif()
  add_library(on1x_diagnostics STATIC src/diagnostics.cpp)
  target_include_directories(on1x_diagnostics PUBLIC include)
  target_link_libraries(on1x_diagnostics PUBLIC on1x_frontend Moonstone::Syren)
  target_link_libraries(on1xc PRIVATE on1x_diagnostics)
  target_compile_definitions(on1xc PRIVATE ON1X_WITH_SYREN=1)
endif()
if(ON1X_WITH_CHISEL)
  add_library(on1x_safety STATIC src/safety.cpp)
  target_include_directories(on1x_safety PUBLIC include)
  target_link_libraries(on1x_safety PUBLIC on1x_frontend Moonstone::Chisel)
  # The supplied public SDK headers contain warnings under strict host flags.
  set_property(TARGET moonstone_core PROPERTY SYSTEM TRUE)
  set_property(TARGET moonstone_chisel PROPERTY SYSTEM TRUE)
  target_link_libraries(on1xc PRIVATE on1x_safety)
  target_compile_definitions(on1xc PRIVATE ON1X_WITH_CHISEL=1)
endif()
if(ON1X_WITH_EXOML)
  add_executable(on1x-bind tools/on1x-bind.cpp)
  target_link_libraries(on1x-bind PRIVATE Moonstone::ExoML)
  if(ON1X_WITH_EXOML_EXTRACT)
    target_link_libraries(on1x-bind PRIVATE Moonstone::ExoMLExtract)
    target_compile_definitions(on1x-bind PRIVATE ON1X_EXOML_EXTRACT=1)
  endif()
endif()

if(UNIX)
  add_executable(on1x-mune-runner stdlib/on1xfile/mune_runner.c)
  target_include_directories(on1x-mune-runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
  target_link_libraries(on1x-mune-runner PRIVATE m ${CMAKE_DL_LIBS})
  add_dependencies(on1xc on1x-mune-runner)
  install(TARGETS on1x-mune-runner RUNTIME DESTINATION libexec/on1x)

  if(ON1X_WITH_PROJECT_QMR)
    if(NOT TARGET Moonstone::QmrCompiler
       AND EXISTS "${ON1X_LIBSTONE_ROOT}/moonstone/qmr/CMakeLists.txt"
       AND EXISTS "${ON1X_LIBSTONE_ROOT}/third_party/lua/lua.h"
       AND EXISTS "${ON1X_LIBSTONE_ROOT}/third_party/kaguya/include/kaguya/kaguya.hpp")
      include(GNUInstallDirs)
      set(QMR_BUILD_TOOLS OFF)
      set(QMR_BUILD_TESTS OFF)
      add_subdirectory("${ON1X_LIBSTONE_ROOT}/moonstone/qmr" "${CMAKE_CURRENT_BINARY_DIR}/project-qmr" EXCLUDE_FROM_ALL)
    endif()
    if(TARGET Moonstone::QmrCompiler)
      add_executable(on1x-qmr-compile stdlib/on1xfile/qmr_compile.cpp)
      target_link_libraries(on1x-qmr-compile PRIVATE Moonstone::QmrCompiler)
      add_dependencies(on1xc on1x-qmr-compile)
      install(TARGETS on1x-qmr-compile RUNTIME DESTINATION libexec/on1x)
    else()
      message(STATUS "On1x project QMR unavailable under ON1X_LIBSTONE_ROOT")
    endif()
  endif()
endif()

if(BUILD_TESTING)
  add_test(NAME on1x.addon_chatgpt COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/addons/chatgpt/test_chatgpt.py")
  set_tests_properties(on1x.addon_chatgpt PROPERTIES TIMEOUT 60)
  # Editor tests are optional; none of these tools are build dependencies.
  add_test(NAME on1x.addon_bat COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/addons/bat/test_bat.py")
  set_tests_properties(on1x.addon_bat PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
  find_program(ON1X_ADDON_NODE node)
  if(ON1X_ADDON_NODE)
    add_test(NAME on1x.addon_vscode_syntax COMMAND "${ON1X_ADDON_NODE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/addons/vscode/test-syntax.cjs")
    set_tests_properties(on1x.addon_vscode_syntax PROPERTIES TIMEOUT 15)
  endif()
  find_program(ON1X_ADDON_NVIM nvim)
  if(UNIX)
    # The server caps compiler address space below what shadow-memory
    # sanitizers reserve. Always run the subprocess/protocol fixtures.
    set(_on1x_addon_compiler_default ON)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _on1x_build_type)
    if("${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_on1x_build_type}}"
        MATCHES "-fsanitize=([^ ]*,)?(address|thread|memory|hwaddress)([, ]|$)")
      set(_on1x_addon_compiler_default OFF)
    endif()
    option(ON1X_TEST_ADDONS_WITH_COMPILER "Run editor addon tests with the built on1xc"
      ${_on1x_addon_compiler_default})
    set(_on1x_lsp_test_args)
    if(ON1X_TEST_ADDONS_WITH_COMPILER)
      list(APPEND _on1x_lsp_test_args --on1xc "$<TARGET_FILE:on1xc>")
      if(NOT ON1X_WITH_SYREN)
        list(APPEND _on1x_lsp_test_args --no-syren)
      endif()
    endif()
    add_test(NAME on1x.addon_lsp COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/addons/lsp/test_lsp.py" ${_on1x_lsp_test_args})
    set_tests_properties(on1x.addon_lsp PROPERTIES TIMEOUT 60)
    find_program(ON1X_ADDON_VIM vim)
    foreach(_editor IN ITEMS VIM NVIM)
      if(ON1X_ADDON_${_editor})
        string(TOLOWER "${_editor}" _editor_name)
        add_test(NAME on1x.addon_${_editor_name} COMMAND sh
          "${CMAKE_CURRENT_SOURCE_DIR}/addons/neovim/tests/run.sh"
          "${ON1X_ADDON_${_editor}}")
        set_tests_properties(on1x.addon_${_editor_name} PROPERTIES TIMEOUT 30)
      endif()
    endforeach()
    if(ON1X_TEST_ADDONS_WITH_COMPILER AND ON1X_WITH_SYREN AND ON1X_ADDON_NVIM)
      execute_process(COMMAND "${ON1X_ADDON_NVIM}" --version
        OUTPUT_VARIABLE _on1x_nvim_version ERROR_QUIET TIMEOUT 5)
      if(_on1x_nvim_version MATCHES "^NVIM v([0-9]+\\.[0-9]+\\.[0-9]+)"
          AND CMAKE_MATCH_1 VERSION_GREATER_EQUAL "0.10")
        add_test(NAME on1x.addon_neovim_lsp COMMAND "${ON1X_ADDON_NVIM}"
          --headless -u NONE -i NONE -l "${CMAKE_CURRENT_SOURCE_DIR}/addons/tests/neovim_lsp.lua"
          "$<TARGET_FILE:on1xc>" "${Python3_EXECUTABLE}")
        set_tests_properties(on1x.addon_neovim_lsp PROPERTIES TIMEOUT 30)
      endif()
    endif()
  endif()
  if(TARGET on1x_grammarembed_bridge)
    add_executable(on1x_grammarembed_reference tests/grammarembed_reference.cpp)
    target_include_directories(on1x_grammarembed_reference SYSTEM PRIVATE "${ON1X_LIBSTONE_ROOT}/firestone")
    add_executable(on1x_grammarembed_bridge_tests tests/test_grammarembed_bridge.cpp)
    target_link_libraries(on1x_grammarembed_bridge_tests PRIVATE on1x_grammarembed_bridge)
    add_test(NAME on1x.grammarembed_bridge COMMAND on1x_grammarembed_bridge_tests)
    set_tests_properties(on1x.grammarembed_bridge PROPERTIES TIMEOUT 20)
  endif()
  if(ON1X_BUILD_NATIVE_PPX)
    add_executable(on1x_luaembed_runtime_tests tests/test_luaembed_runtime.c)
    target_link_libraries(on1x_luaembed_runtime_tests PRIVATE on1x_luaembed_bridge)
    add_test(NAME on1x.luaembed_runtime COMMAND on1x_luaembed_runtime_tests)
    set_tests_properties(on1x.luaembed_runtime PROPERTIES TIMEOUT 30)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_executable(on1x_luaembed_allocation_tests tests/test_luaembed_allocation.c)
      target_link_libraries(on1x_luaembed_allocation_tests PRIVATE on1x_luaembed_bridge)
      target_link_options(on1x_luaembed_allocation_tests PRIVATE
        "-Wl,--wrap=malloc" "-Wl,--wrap=calloc" "-Wl,--wrap=realloc")
      add_test(NAME on1x.luaembed_allocation COMMAND on1x_luaembed_allocation_tests)
      set_tests_properties(on1x.luaembed_allocation PROPERTIES TIMEOUT 60)
    endif()
    add_executable(on1x_extension_evidence_tests tests/test_extension_evidence.cpp)
    target_link_libraries(on1x_extension_evidence_tests PRIVATE on1x_frontend)
    add_dependencies(on1x_extension_evidence_tests on1x_category_pin)
    add_test(NAME on1x.extension_evidence_hir COMMAND on1x_extension_evidence_tests
      "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/examples/category/eq.on1x")
    set_tests_properties(on1x.extension_evidence_hir PROPERTIES TIMEOUT 60)
  endif()
  if(ON1X_WITH_SYREN)
    add_executable(on1x_diagnostics_tests tests/test_diagnostics.cpp)
    target_link_libraries(on1x_diagnostics_tests PRIVATE on1x_diagnostics)
    add_test(NAME on1x.diagnostics_api COMMAND on1x_diagnostics_tests)
  endif()
  add_executable(on1x_analysis_tests tests/test_analysis.cpp)
  target_link_libraries(on1x_analysis_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.analysis_api COMMAND on1x_analysis_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/diagnostics/catalog.json")
  add_test(NAME on1x.diagnostic_catalog_source COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_diagnostic_catalog_source.py"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/diagnostic-catalog-source"
    "${CMAKE_CXX_COMPILER}")
  set_tests_properties(on1x.diagnostic_catalog_source PROPERTIES TIMEOUT 30)
  add_executable(on1x_loader_tests tests/test_loader.cpp)
  target_link_libraries(on1x_loader_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.loader_api COMMAND on1x_loader_tests "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/loader-api")
  if(ON1X_WITH_NEROTYPICAL)
    add_executable(on1x_type_safety_tests tests/test_type_safety.cpp)
    target_link_libraries(on1x_type_safety_tests PRIVATE on1x_type_safety)
    add_test(NAME on1x.type_safety_hir COMMAND on1x_type_safety_tests)
    set_tests_properties(on1x.type_safety_hir PROPERTIES TIMEOUT 60)
  endif()
  if(ON1X_WITH_CHISEL)
    add_executable(on1x_safety_tests tests/test_safety.cpp)
    target_link_libraries(on1x_safety_tests PRIVATE on1x_safety)
    add_test(NAME on1x.safety_hir COMMAND on1x_safety_tests)
    set_tests_properties(on1x.safety_hir PROPERTIES TIMEOUT 60)
    add_executable(on1x_external_memory_example examples/external-memory.cpp)
    target_link_libraries(on1x_external_memory_example PRIVATE on1x_safety)
    add_test(NAME on1x.external_memory_model COMMAND on1x_external_memory_example)
  endif()
  add_executable(on1x_pass_tests tests/test_passes.cpp)
  target_link_libraries(on1x_pass_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.passes_hir COMMAND on1x_pass_tests)
  set_tests_properties(on1x.passes_hir PROPERTIES TIMEOUT 120)
  if(ON1X_WITH_NOVOSTONE)
    add_executable(on1x_relational_tests tests/test_relational.cpp)
    target_link_libraries(on1x_relational_tests PRIVATE on1x_novostone)
    add_test(NAME on1x.relational_hir COMMAND on1x_relational_tests)
    set_tests_properties(on1x.relational_hir PROPERTIES TIMEOUT 120)
  endif()
  add_executable(on1x_mir_tests tests/test_mir.cpp)
  target_link_libraries(on1x_mir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.mir_hir COMMAND on1x_mir_tests)
  set_tests_properties(on1x.mir_hir PROPERTIES TIMEOUT 40)
  find_package(Threads REQUIRED)
  add_executable(on1x_scanner_tests tests/test_scanner.cpp)
  target_link_libraries(on1x_scanner_tests PRIVATE on1x_frontend Threads::Threads)
  add_test(NAME on1x.scanner COMMAND on1x_scanner_tests)
  set_tests_properties(on1x.scanner PROPERTIES TIMEOUT 40)
  add_executable(on1x_type_tests tests/test_types.cpp)
  target_link_libraries(on1x_type_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.types COMMAND on1x_type_tests)
  set_tests_properties(on1x.types PROPERTIES TIMEOUT 40)
  add_executable(on1x_inference_tests tests/test_infer.cpp)
  target_link_libraries(on1x_inference_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.inference_engine COMMAND on1x_inference_tests)
  set_tests_properties(on1x.inference_engine PROPERTIES TIMEOUT 40)
  add_executable(on1x_type_checker_hardening_tests
    tests/test_type_checker_hardening.cpp)
  target_link_libraries(on1x_type_checker_hardening_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.type_checker_hardening_hir
    COMMAND on1x_type_checker_hardening_tests)
  set_tests_properties(on1x.type_checker_hardening_hir PROPERTIES TIMEOUT 40)
  add_executable(on1x_closure_hir_tests tests/test_closures.cpp)
  target_link_libraries(on1x_closure_hir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.closure_hir COMMAND on1x_closure_hir_tests)
  set_tests_properties(on1x.closure_hir PROPERTIES TIMEOUT 40)
  add_executable(on1x_tuple_hir_tests tests/test_tuples.cpp)
  target_link_libraries(on1x_tuple_hir_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.tuple_hir COMMAND on1x_tuple_hir_tests)
  set_tests_properties(on1x.tuple_hir PROPERTIES TIMEOUT 40)
  if(ON1X_GENERATED_FRONTEND)
    # Differential scanner regression: the generated Aurocks scanner must
    # match independent fallback tokenization, including byte offsets.
    add_library(on1x_reference_scanner STATIC src/lexer_fallback.cpp)
    target_include_directories(on1x_reference_scanner PRIVATE include)
    add_executable(on1x_reference_tokens tests/reference_tokens.cpp src/lexer.cpp src/source.cpp)
    target_include_directories(on1x_reference_tokens PRIVATE include)
    target_link_libraries(on1x_reference_tokens PRIVATE on1x_reference_scanner)
    add_executable(on1x_generated_tokens tests/reference_tokens.cpp)
    target_link_libraries(on1x_generated_tokens PRIVATE on1x_frontend)
  endif()
  add_test(NAME on1x.formatter COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_formatter.py"
    "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}")
  set_tests_properties(on1x.formatter PROPERTIES TIMEOUT 120)
  if(TARGET on1x-mune-runner)
    set(_project_qmr_test_tool)
    if(TARGET on1x-qmr-compile)
      set(_project_qmr_test_tool "$<TARGET_FILE:on1x-qmr-compile>")
    endif()
    add_test(NAME on1x.project_helpers COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_project_helpers.py"
      "$<TARGET_FILE:on1x-mune-runner>" "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/on1xfile"
      ${_project_qmr_test_tool})
    set_tests_properties(on1x.project_helpers PROPERTIES TIMEOUT 30)
  endif()
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    set(_project_test_tools --mune-runner "$<TARGET_FILE:on1x-mune-runner>")
    if(TARGET on1x-qmr-compile)
      list(APPEND _project_test_tools --qmr-compiler "$<TARGET_FILE:on1x-qmr-compile>")
    endif()
    add_test(NAME on1x.project_build COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_on1x_project.py" --on1xc "$<TARGET_FILE:on1xc>"
      --cc "${CMAKE_C_COMPILER}" ${_project_test_tools})
    set_tests_properties(on1x.project_build PROPERTIES TIMEOUT 300)
    add_test(NAME on1x.project_fetch COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_project_fetch.py" --on1xc "$<TARGET_FILE:on1xc>"
      --cc "${CMAKE_C_COMPILER}")
    set_tests_properties(on1x.project_fetch PROPERTIES TIMEOUT 300)
  endif()
  set(_novostone_test_options)
  if(ON1X_WITH_NOVOSTONE)
    list(APPEND _novostone_test_options --enabled)
  endif()
  add_test(NAME on1x.relational COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_relational.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/relational" ${_novostone_test_options})
  set_tests_properties(on1x.relational PROPERTIES TIMEOUT 60)
  set(_test_c_flags)
  foreach(_flag IN LISTS ON1X_TEST_C_FLAGS)
    list(APPEND _test_c_flags "--cflag=${_flag}")
  endforeach()
  add_test(NAME on1x.diagnostic_catalog_runtime COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_diagnostic_catalog_runtime.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/diagnostic-catalog-runtime"
    "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.diagnostic_catalog_runtime PROPERTIES TIMEOUT 60)
  if(ON1X_WITH_SYREN)
    add_test(NAME on1x.diagnostics COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_diagnostics.py" "$<TARGET_FILE:on1xc>"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/diagnostics"
      "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.diagnostics PROPERTIES TIMEOUT 60)
  endif()
  add_test(NAME on1x.compiler COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compiler.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/compiler" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.limits COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_limits.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/limits" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.functions COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_functions.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/functions" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.json COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_json.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/json" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.inference COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inference.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/inference" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.closures COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_closures.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/closures" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.tuples COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tuples.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/tuples" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.parameter_patterns COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parameter_patterns.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/parameter-patterns" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  add_test(NAME on1x.data COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/data" "${CMAKE_C_COMPILER}"
    ${_test_c_flags})
  set_tests_properties(on1x.compiler on1x.limits on1x.functions on1x.json on1x.inference on1x.closures on1x.tuples on1x.parameter_patterns on1x.data PROPERTIES TIMEOUT 120)
  set_tests_properties(on1x.json PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME on1x.poly COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_poly.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/poly" "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.poly PROPERTIES TIMEOUT 120)
  add_test(NAME on1x.poly_extended COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_poly_extended.py" "$<TARGET_FILE:on1xc>"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/poly-extended" "${CMAKE_C_COMPILER}" ${_test_c_flags})
  set_tests_properties(on1x.poly_extended PROPERTIES TIMEOUT 120)
  add_executable(on1x_meta_tests tests/test_meta.cpp)
  target_link_libraries(on1x_meta_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.meta_hir COMMAND on1x_meta_tests)
  add_executable(on1x_control_tests tests/test_control.cpp)
  target_link_libraries(on1x_control_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.control_hir COMMAND on1x_control_tests)
  add_executable(on1x_pattern_tests tests/test_patterns.cpp)
  target_link_libraries(on1x_pattern_tests PRIVATE on1x_frontend)
  add_test(NAME on1x.pattern_hir COMMAND on1x_pattern_tests)
  add_executable(on1x_value_loop_tests tests/test_value_loops.cpp)
  target_link_libraries(on1x_value_loop_tests PRIVATE on1x_frontend)
   add_test(NAME on1x.value_loop_hir COMMAND on1x_value_loop_tests)
     add_executable(on1x_nominal_tests tests/test_nominal_adts.cpp)
     target_link_libraries(on1x_nominal_tests PRIVATE on1x_frontend)
     add_test(NAME on1x.nominal_hir COMMAND on1x_nominal_tests)
     add_executable(on1x_recursive_owning_adts_tests
       tests/test_recursive_owning_adts.cpp)
     target_link_libraries(on1x_recursive_owning_adts_tests
       PRIVATE on1x_frontend on1x_runtime)
     add_test(NAME on1x.recursive_owning_adts_hir
       COMMAND on1x_recursive_owning_adts_tests)
     set_tests_properties(on1x.recursive_owning_adts_hir
       PROPERTIES TIMEOUT 60)
     add_executable(on1x_cliche_tests tests/test_cliche.cpp)
     target_link_libraries(on1x_cliche_tests PRIVATE on1x_frontend)
     add_test(NAME on1x.cliche_hir COMMAND on1x_cliche_tests)
     set_tests_properties(on1x.cliche_hir PROPERTIES TIMEOUT 60)
     add_executable(on1x_core_impl_tests tests/test_core_impl.cpp)
     target_link_libraries(on1x_core_impl_tests PRIVATE on1x_frontend)
     add_test(NAME on1x.core_impl_hir COMMAND on1x_core_impl_tests)
     set_tests_properties(on1x.core_impl_hir PROPERTIES TIMEOUT 60)
    add_executable(on1x_defer_error_tests tests/test_defer_errors.cpp)
    target_link_libraries(on1x_defer_error_tests PRIVATE on1x_frontend)
    add_test(NAME on1x.defer_error_hir COMMAND on1x_defer_error_tests)
    add_executable(on1x_concurrency_hir_tests tests/test_concurrency.cpp)
    target_link_libraries(on1x_concurrency_hir_tests PRIVATE on1x_frontend)
    add_test(NAME on1x.concurrency_hir COMMAND on1x_concurrency_hir_tests)
   set(_meta_tests macros ppx staging selfhost local_syntax contracts mir passes modules prelude libraries stdppx install mulib stdlib_extensions tty container_package f64 unsigned_f32 numeric_limits)
  if(ON1X_BUILD_NATIVE_PPX)
    list(APPEND _meta_tests luaembed)
    if(ON1X_BUILD_GRAMMAREMBED)
      list(APPEND _meta_tests grammarembed)
    endif()
  endif()
     list(APPEND _meta_tests category control patterns value_loops nominal_adts cliche core_impl defer_errors concurrency stream_poll cancellation_deadlines taskgroup_within discovery)
     list(APPEND _meta_tests kernel_imports project_driver enum_connective core_ref container_local container_shared
      literals_core owning_adt recursive_owning_adts type_checker_hardening bounded_kernel_inventory)
   if(ON1X_HAS_FIRESTONE_REGEX)
     list(APPEND _meta_tests core_syntax_integration)
     add_test(NAME on1x.core_regex COMMAND "${Python3_EXECUTABLE}"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core_regex.py"
       "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" ${_test_c_flags})
     set_tests_properties(on1x.core_regex PROPERTIES TIMEOUT 180)
   endif()
   add_executable(on1x_core_ref_tests tests/test_core_ref.cpp)
   target_link_libraries(on1x_core_ref_tests PRIVATE on1x_frontend)
   add_test(NAME on1x.core_ref_hir COMMAND on1x_core_ref_tests)
    add_executable(on1x_kernel_ipc_hir_tests tests/test_kernel_ipc_hir.cpp)
    target_link_libraries(on1x_kernel_ipc_hir_tests PRIVATE on1x_frontend)
     add_test(NAME on1x.kernel_ipc_hir COMMAND on1x_kernel_ipc_hir_tests
       "${CMAKE_CURRENT_SOURCE_DIR}/examples/kernel/framed_pair.on1x")
    add_executable(on1x_container_shared_hir_tests tests/test_container_shared_hir.cpp)
    target_link_libraries(on1x_container_shared_hir_tests PRIVATE on1x_frontend)
    add_test(NAME on1x.container_shared_hir COMMAND on1x_container_shared_hir_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/container_shared/lifecycle.on1x"
      "${CMAKE_CURRENT_SOURCE_DIR}/stdlib")
    add_test(NAME on1x.container_native COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_container_native.py"
      "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/container-native" ${_test_c_flags})
     set_tests_properties(on1x.container_native PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
   if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
     foreach(_pod_mode native namespace cgroup on1x)
       add_test(NAME on1x.container_pod_${_pod_mode} COMMAND "${Python3_EXECUTABLE}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_container_pod.py" "${_pod_mode}"
         "${CMAKE_C_COMPILER}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/container-pod"
         --on1xc "$<TARGET_FILE:on1xc>" ${_test_c_flags})
       set_tests_properties(on1x.container_pod_${_pod_mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
     endforeach()
   endif()
  add_test(NAME on1x.stdppx_protocol COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/stdppx/tests" -v)
  if(ON1X_WITH_EXOTRACT)
    add_test(NAME on1x.exotract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_exotract.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/exotract" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.exotract PROPERTIES TIMEOUT 180)
    add_test(NAME on1x.kernel_paths COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_paths.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-paths" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_paths PROPERTIES TIMEOUT 180)
  endif()
  if(ON1X_WITH_EXOTRACT_LUA)
    add_test(NAME on1x.bounded_kernel_discovery_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bounded_kernel_discovery_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/bounded-kernel-discovery-sdk" "${CMAKE_C_COMPILER}")
    set_tests_properties(on1x.bounded_kernel_discovery_sdk PROPERTIES TIMEOUT 360 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.cdd_discovery_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cdd_discovery_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/cdd-discovery-sdk" "${CMAKE_C_COMPILER}")
    set_tests_properties(on1x.cdd_discovery_sdk PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.kernel_sandbox_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_sandbox_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-sandbox-sdk" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_sandbox_sdk PROPERTIES TIMEOUT 240 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.kernel_imports_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_imports_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-imports-sdk" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_imports_sdk PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.kernel_review_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_review_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-review-sdk" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_review_sdk PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.kernel_history_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_history_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-history-sdk" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_history_sdk PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
    add_test(NAME on1x.kernel_ipc_sdk COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kernel_ipc_sdk.py"
      "$<TARGET_FILE:exotract>" "$<TARGET_FILE:on1xc>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/kernel-ipc-sdk"
      "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.kernel_ipc_sdk PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
  endif()
  if(ON1X_WITH_NEROTYPICAL)
    list(APPEND _meta_tests type_safety)
  endif()
  if(ON1X_WITH_CHISEL)
    list(APPEND _meta_tests safety)
  endif()
   foreach(_meta_test ${_meta_tests})
    add_test(NAME on1x.${_meta_test} COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${_meta_test}.py" "$<TARGET_FILE:on1xc>"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/${_meta_test}" "${CMAKE_C_COMPILER}" ${_test_c_flags})
    set_tests_properties(on1x.${_meta_test} PROPERTIES TIMEOUT 180)
   endforeach()
   set_tests_properties(on1x.recursive_owning_adts PROPERTIES TIMEOUT 300)
  set_tests_properties(on1x.contracts PROPERTIES SKIP_RETURN_CODE 77)
  set_tests_properties(on1x.category PROPERTIES SKIP_RETURN_CODE 77)
  if(ON1X_WITH_EXOML)
    set(_contract_extract_test)
    if(ON1X_WITH_EXOML_EXTRACT)
      set(_contract_extract_test --extract)
    endif()
    add_test(NAME on1x.contracts_sdk COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_contracts_sdk.py" "$<TARGET_FILE:on1xc>"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/contracts-sdk" "${CMAKE_C_COMPILER}"
      "$<TARGET_FILE:on1x-bind>" ${_contract_extract_test} ${_test_c_flags})
    set_tests_properties(on1x.contracts_sdk PROPERTIES TIMEOUT 240)
    if(ON1X_WITH_EXOML_EXTRACT AND ON1X_WITH_CHISEL)
      add_test(NAME on1x.contract_strings COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_contract_strings.py" "$<TARGET_FILE:on1xc>"
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/contract-strings" "${CMAKE_C_COMPILER}"
        "$<TARGET_FILE:on1x-bind>" ${_test_c_flags})
      set_tests_properties(on1x.contract_strings PROPERTIES TIMEOUT 180)
    endif()
  endif()
  if(ON1X_GENERATED_FRONTEND)
    add_test(NAME on1x.scanner_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/scanner_parity.py"
      "$<TARGET_FILE:on1x_generated_tokens>" "$<TARGET_FILE:on1x_reference_tokens>"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/parity")
    set_tests_properties(on1x.scanner_parity PROPERTIES TIMEOUT 120)
  endif()
endif()

# SDK 2 integration fragments are evaluated only after every canonical compiler,
# runtime, backend and test target above has been declared. Subsystem workers own
# one numbered fragment each; the principal retains this aggregate file.
if(ON1X_BUILD_EXTENSION_SDK)
  foreach(_on1x_sdk_subagent RANGE 1 8)
    include("${CMAKE_CURRENT_SOURCE_DIR}/sdk/cmake/Integration${_on1x_sdk_subagent}.cmake" OPTIONAL)
  endforeach()
  if(BUILD_TESTING AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
      AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME on1x.sdk.aggregate_headers
      COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_aggregate_headers.py"
        "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime-sdk/include"
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/sdk-aggregate-headers")
    set_tests_properties(on1x.sdk.aggregate_headers PROPERTIES TIMEOUT 300)
  endif()
  if(BUILD_TESTING AND UNIX AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME on1x.sdk.cli_dispatch
      COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/test_cli_dispatch.py"
        "$<TARGET_FILE:on1xc>")
    set_tests_properties(on1x.sdk.cli_dispatch PROPERTIES TIMEOUT 60)
  endif()
endif()
