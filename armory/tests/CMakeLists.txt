cmake_minimum_required(VERSION 3.24)

# Also usable independently: cmake -S armory/tests -B build/armory-tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ArmoryEnumTests LANGUAGES CXX)
    enable_testing()
    add_library(Armory INTERFACE)
    target_include_directories(Armory INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_features(Armory INTERFACE cxx_std_20)
    include(GNUInstallDirs)
    include(../cmake/Mulib.cmake)
    if(MULL_ENABLE_MULIB)
        target_link_libraries(Armory INTERFACE mull-mulib-bridge)
    endif()
endif()

# The suite uses assertions as checks, including in optimized configurations.
if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

# Optional legacy tests are usable when their separate sources are present.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../MetaML.hpp")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(../cmake/QBE.cmake)
endif()
# MetaML language, portable host modules, and a genuinely C-compiled client.
option(Armory_TEST_D "Test the D module when a D compiler is available" ON)
enable_language(C)
find_package(Threads REQUIRED)
if(NOT TARGET metaml-repl-cpp)
    add_executable(metaml-repl-cpp ../metaml-cli/metaml.cpp)
    target_link_libraries(metaml-repl-cpp PRIVATE Armory Threads::Threads)
endif()
add_executable(metaml_runtime_tests test_metaml.cpp)
target_link_libraries(metaml_runtime_tests PRIVATE Armory Threads::Threads)
add_test(NAME metaml_runtime COMMAND metaml_runtime_tests ${CMAKE_CURRENT_SOURCE_DIR}/../metalib)

add_executable(metalib_extensions_tests test_metalib_extensions.cpp)
find_path(METAML_BOOST_INCLUDE_DIR boost/multiprecision/cpp_int.hpp REQUIRED)
target_include_directories(metalib_extensions_tests PRIVATE ${METAML_BOOST_INCLUDE_DIR})
target_link_libraries(metalib_extensions_tests PRIVATE Armory Threads::Threads ${CMAKE_DL_LIBS})
if(METAML_BUILD_SERVICES)
    target_link_libraries(metalib_extensions_tests PRIVATE metaml-services)
    target_compile_definitions(metalib_extensions_tests PRIVATE METAML_TEST_SERVICES=1)
endif()
add_test(NAME metalib_extensions COMMAND metalib_extensions_tests ${CMAKE_CURRENT_SOURCE_DIR}/../metalib)

add_executable(metalib_toolchain_tests test_metalib_toolchain.cpp)
target_link_libraries(metalib_toolchain_tests PRIVATE Armory Threads::Threads)
add_test(NAME metalib_toolchain COMMAND metalib_toolchain_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../metalib ${CMAKE_CURRENT_BINARY_DIR})

add_executable(metaml_qbe_tests test_metaml_qbe.cpp)
target_link_libraries(metaml_qbe_tests PRIVATE Armory Threads::Threads)
add_test(NAME metaml_qbe COMMAND metaml_qbe_tests)

if(NOT TARGET metaml-c)
    add_library(metaml-c STATIC ../metalib/c/MetaML.cpp)
    target_link_libraries(metaml-c PUBLIC Armory Threads::Threads)
endif()
add_executable(metaml_c_tests test_metaml_c.c)
target_link_libraries(metaml_c_tests PRIVATE metaml-c)
set_target_properties(metaml_c_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
add_test(NAME metaml_c_api COMMAND metaml_c_tests ${CMAKE_CURRENT_SOURCE_DIR}/../metalib)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_metaml_unit_frontend)
if(TARGET metaml)
    list(APPEND _metaml_unit_frontend --frontend $<TARGET_FILE:metaml>)
endif()
add_test(NAME metaml_language_units COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_language_units.py --binary $<TARGET_FILE:metaml-repl-cpp> ${_metaml_unit_frontend})
set(_metaml_editor_test_args)
if(TARGET metaml)
    list(APPEND _metaml_editor_test_args --metaml $<TARGET_FILE:metaml>)
endif()
add_test(NAME metaml_editor_addons COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../addons/tests/check_addons.py ${_metaml_editor_test_args})
if(TARGET metaml)
    add_test(NAME metaml_replxx COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_replxx.py --binary $<TARGET_FILE:metaml>)
    if(TARGET metaml-mammal)
        add_test(NAME metaml_mammal COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_mammal.py
            --metaml $<TARGET_FILE:metaml> --mammal $<TARGET_FILE:metaml-mammal>
            --legacy $<TARGET_FILE:metaml-repl-cpp>)
    endif()
endif()
add_test(NAME metaml_cli COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_cli.py --binary $<TARGET_FILE:metaml-repl-cpp>)
set(_metaml_xvm_args --cpp $<TARGET_FILE:metaml-repl-cpp>)
find_program(METAML_D_COMPILER NAMES ldc2 dmd gdc)
set(_metaml_d_qbe_flags)
set(_metaml_d_qbe_deps)
if(METAML_ENABLE_QBE)
    set(_metaml_d_qbe_deps metaml-qbe)
    set(_metaml_d_qbe_cxx_runtime stdc++)
    if("c++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
        set(_metaml_d_qbe_cxx_runtime c++)
    endif()
    if(METAML_D_COMPILER MATCHES "gdc")
        set(_metaml_d_qbe_flags -fversion=METAML_ENABLE_QBE $<TARGET_FILE:metaml-qbe>)
        foreach(_lib IN ITEMS ${_metaml_d_qbe_cxx_runtime} ${CMAKE_DL_LIBS})
            list(APPEND _metaml_d_qbe_flags -l${_lib})
        endforeach()
    elseif(METAML_D_COMPILER MATCHES "ldc")
        set(_metaml_d_qbe_flags -d-version=METAML_ENABLE_QBE $<TARGET_FILE:metaml-qbe>)
    else()
        set(_metaml_d_qbe_flags -version=METAML_ENABLE_QBE $<TARGET_FILE:metaml-qbe>)
    endif()
    if(NOT METAML_D_COMPILER MATCHES "gdc")
        foreach(_lib IN ITEMS ${_metaml_d_qbe_cxx_runtime} ${CMAKE_DL_LIBS})
            list(APPEND _metaml_d_qbe_flags -L-l${_lib})
        endforeach()
    endif()
endif()
if(METAML_D_COMPILER AND Armory_TEST_D)
    file(GLOB _metaml_d_modules CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../metalib/d/*.d)
    # Some reserved module files are still empty placeholders, not D modules.
    foreach(_source IN LISTS _metaml_d_modules)
        file(SIZE ${_source} _size)
        if(_size GREATER 0)
            list(APPEND _metaml_d_sources ${_source})
        endif()
    endforeach()
    file(GLOB _metaml_d_adapters CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../metalib/*/*.d)
    list(FILTER _metaml_d_adapters EXCLUDE REGEX "/metalib/d/")
    list(APPEND _metaml_d_sources ${_metaml_d_adapters})
    list(APPEND _metaml_d_sources ${CMAKE_CURRENT_SOURCE_DIR}/../metaml-backends/QBE.d)
    foreach(_kind IN ITEMS repl tests extensions toolchain qbe)
        if(_kind STREQUAL repl)
            set(_main ${CMAKE_CURRENT_SOURCE_DIR}/../metaml-cli/metaml.d)
            set(_bin ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
        elseif(_kind STREQUAL tests)
            set(_main ${CMAKE_CURRENT_SOURCE_DIR}/test_metaml.d)
            set(_bin ${CMAKE_CURRENT_BINARY_DIR}/metaml_runtime_d_tests)
        elseif(_kind STREQUAL extensions)
            set(_main ${CMAKE_CURRENT_SOURCE_DIR}/test_metalib_extensions.d)
            set(_bin ${CMAKE_CURRENT_BINARY_DIR}/metalib_extensions_d_tests)
        elseif(_kind STREQUAL toolchain)
            set(_main ${CMAKE_CURRENT_SOURCE_DIR}/test_metalib_toolchain.d)
            set(_bin ${CMAKE_CURRENT_BINARY_DIR}/metalib_toolchain_d_tests)
        else()
            set(_main ${CMAKE_CURRENT_SOURCE_DIR}/test_metaml_qbe.d)
            set(_bin ${CMAKE_CURRENT_BINARY_DIR}/metaml_qbe_d_tests)
        endif()
        if(METAML_D_COMPILER MATCHES "gdc")
            set(_flags -o ${_bin})
        else()
            set(_flags -of=${_bin})
        endif()
        add_custom_command(OUTPUT ${_bin}
            COMMAND ${METAML_D_COMPILER} ${_flags} -I${CMAKE_CURRENT_SOURCE_DIR}/..
                ${_metaml_d_qbe_flags}
                ${_main} ${CMAKE_CURRENT_SOURCE_DIR}/../MetaML.d
                ${CMAKE_CURRENT_SOURCE_DIR}/../Armory.d ${_metaml_d_sources}
            DEPENDS ${_main} ../MetaML.d ../Armory.d ../XVMCode.d ../XVMNative.d ../QBENative.d
                ${_metaml_d_sources} ${_metaml_d_qbe_deps}
            VERBATIM)
        add_custom_target(metaml_d_${_kind}_build ALL DEPENDS ${_bin})
    endforeach()
    add_test(NAME metaml_runtime_d COMMAND ${CMAKE_CURRENT_BINARY_DIR}/metaml_runtime_d_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../metalib)
    add_test(NAME metaml_language_units_d COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_language_units.py --binary ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
    add_test(NAME metalib_extensions_d COMMAND ${CMAKE_CURRENT_BINARY_DIR}/metalib_extensions_d_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../metalib)
    add_test(NAME metalib_toolchain_d COMMAND ${CMAKE_CURRENT_BINARY_DIR}/metalib_toolchain_d_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../metalib ${CMAKE_CURRENT_BINARY_DIR})
    add_test(NAME metaml_qbe_d COMMAND ${CMAKE_CURRENT_BINARY_DIR}/metaml_qbe_d_tests)
    add_test(NAME metaml_cli_d COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_cli.py
        --binary ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
    if(TARGET metaml AND TARGET metaml-mammal)
        add_test(NAME metaml_mammal_d COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_mammal.py
            --metaml $<TARGET_FILE:metaml> --mammal $<TARGET_FILE:metaml-mammal>
            --legacy ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
    endif()
    add_test(NAME metaml_parity COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/parity_check.py
        --cpp $<TARGET_FILE:metaml-repl-cpp> --d ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
    add_test(NAME metaml_manual_d COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_manual.py
        --cpp $<TARGET_FILE:metaml-repl-cpp> --d ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
    list(APPEND _metaml_xvm_args --d ${CMAKE_CURRENT_BINARY_DIR}/metaml-repl-d)
endif()
add_test(NAME metaml_xvm_differential COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_xvm.py ${_metaml_xvm_args})
if(METAML_ENABLE_QBE)
    set(_metaml_qbe_cli_args ${_metaml_xvm_args})
    if(TARGET metaml)
        list(APPEND _metaml_qbe_cli_args --metaml $<TARGET_FILE:metaml>)
    endif()
    add_test(NAME metaml_qbe_cli COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_qbe.py ${_metaml_qbe_cli_args})
endif()
add_test(NAME metaml_library_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/run_tests.py --cpp $<TARGET_FILE:metaml-repl-cpp>)
add_test(NAME metaml_manual COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_manual.py --cpp $<TARGET_FILE:metaml-repl-cpp>)
add_test(NAME metaml_manual_generated COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_manual.py --check)
set(_manual_native_args --cxx ${CMAKE_CXX_COMPILER} --cc ${CMAKE_C_COMPILER}
    --library $<TARGET_FILE:metaml-c>)
if(METAML_ENABLE_QBE)
    list(APPEND _manual_native_args --extra-library $<TARGET_FILE:metaml-qbe>)
    foreach(_lib IN ITEMS ${CMAKE_DL_LIBS})
        list(APPEND _manual_native_args --extra-library=-l${_lib})
    endforeach()
endif()
if(METAML_D_COMPILER AND Armory_TEST_D)
    list(APPEND _manual_native_args --d-compiler ${METAML_D_COMPILER})
endif()
add_test(NAME metaml_manual_native COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_native_manual.py ${_manual_native_args})

elseif(METAML_ENABLE_QBE)
    message(STATUS "SKIP: legacy MetaML QBE tests require the MetaML source tree")
endif()

add_executable(armory_enum_tests test_enum_reflection.cpp enum_odr.cpp)
target_link_libraries(armory_enum_tests PRIVATE Armory)
add_test(NAME armory_enum_reflection COMMAND armory_enum_tests)

add_executable(armory_enum_config_tests test_enum_config.cpp)
target_link_libraries(armory_enum_config_tests PRIVATE Armory)
add_test(NAME armory_enum_configuration COMMAND armory_enum_config_tests)

add_executable(armory_enum_alias_tests test_enum_aliases.cpp)
target_link_libraries(armory_enum_alias_tests PRIVATE Armory)
add_test(NAME armory_enum_aliases COMMAND armory_enum_alias_tests)

add_executable(armory_enum_format_tests test_enum_format.cpp)
target_link_libraries(armory_enum_format_tests PRIVATE Armory)
add_test(NAME armory_enum_format COMMAND armory_enum_format_tests)

add_executable(armory_language_tests test_language_facilities.cpp)
target_link_libraries(armory_language_tests PRIVATE Armory)
add_test(NAME armory_language_facilities COMMAND armory_language_tests)

add_executable(armory_predicate_tests test_predicate.cpp)
target_link_libraries(armory_predicate_tests PRIVATE Armory)
add_test(NAME armory_predicate COMMAND armory_predicate_tests)

add_executable(armory_template_tests test_template_facilities.cpp)
target_link_libraries(armory_template_tests PRIVATE Armory)
add_test(NAME armory_template_facilities COMMAND armory_template_tests)

add_executable(armory_task_pipeline_example ../examples/21-task-pipeline.cpp)
target_link_libraries(armory_task_pipeline_example PRIVATE Armory)
add_test(NAME armory_task_pipeline COMMAND armory_task_pipeline_example)

add_executable(armory_graph_term_tests test_graph_terms.cpp)
target_link_libraries(armory_graph_term_tests PRIVATE Armory)
add_test(NAME armory_graph_terms COMMAND armory_graph_term_tests)

add_executable(armory_regression_tests test_regressions.cpp)
target_link_libraries(armory_regression_tests PRIVATE Armory)
add_test(NAME armory_regressions COMMAND armory_regression_tests)
add_executable(mull_tests test_mull.cpp)
target_link_libraries(mull_tests PRIVATE Armory)
add_test(NAME mull_host COMMAND mull_tests)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_mull_parity_args --cpp $<TARGET_FILE:mull_tests>)
add_test(NAME mullier_embedded_rules COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mull/rewrite/embed.py --check)
if(MULL_ENABLE_MULIB)
    list(APPEND _mull_parity_args --mulib)
    add_test(NAME mull_mulib_inventory COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_mulib_inventory.py
        --include ${MULL_MULIB_INCLUDE_DIR})
    add_test(NAME mull_mulib_inventory_regressions COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_mulib_inventory.py)
endif()

option(Armory_TEST_D "Test the D module when a D compiler is available" ON)
if(Armory_TEST_D)
    find_program(Armory_TEST_D_COMPILER NAMES ldc2 dmd gdc)
    if(Armory_TEST_D_COMPILER)
        set(_armory_d_test ${CMAKE_CURRENT_BINARY_DIR}/armory_d_tests${CMAKE_EXECUTABLE_SUFFIX})
        if(Armory_TEST_D_COMPILER MATCHES "gdc")
            set(_armory_d_test_flags -funittest -fmain -o ${_armory_d_test})
        else()
            set(_armory_d_test_flags -unittest -main -of=${_armory_d_test})
        endif()
        add_custom_command(
            OUTPUT ${_armory_d_test}
            COMMAND ${Armory_TEST_D_COMPILER} ${_armory_d_test_flags}
                    -I${CMAKE_CURRENT_SOURCE_DIR}/..
                    ${CMAKE_CURRENT_SOURCE_DIR}/../Armory.d
                    ${CMAKE_CURRENT_SOURCE_DIR}/../mull/mullier/MuLLIER.d
                    ${CMAKE_CURRENT_SOURCE_DIR}/../mull/mullier/DefaultRules.d
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_terms.d
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_regressions.d
            DEPENDS ../Armory.d ../mull/mullier/MuLLIER.d ../mull/mullier/DefaultRules.d test_graph_terms.d test_regressions.d
            VERBATIM)
        add_custom_target(armory_d_test_build ALL DEPENDS ${_armory_d_test})
        add_test(NAME armory_d_module COMMAND ${_armory_d_test})
        file(GLOB _mull_d_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../mull/native/*/Native.d
            ${CMAKE_CURRENT_SOURCE_DIR}/../mull/mullier/*.d)
        list(FILTER _mull_d_sources EXCLUDE REGEX "/Mulib/")
        list(APPEND _mull_d_sources ${CMAKE_CURRENT_SOURCE_DIR}/../mull/stdlib/Stdlib.d)
        set(_mull_d_test ${CMAKE_CURRENT_BINARY_DIR}/mull_d_tests${CMAKE_EXECUTABLE_SUFFIX})
        if(Armory_TEST_D_COMPILER MATCHES "gdc")
            set(_mull_d_flags -o ${_mull_d_test})
        else()
            set(_mull_d_flags -of=${_mull_d_test})
        endif()
        if(MULL_ENABLE_MULIB)
            list(APPEND _mull_d_sources
                ${CMAKE_CURRENT_SOURCE_DIR}/../mull/bridge/Mulib/Native.d
                ${CMAKE_CURRENT_SOURCE_DIR}/../mull/bridge/Mulib/Bridge.d)
            if(Armory_TEST_D_COMPILER MATCHES "gdc")
                list(APPEND _mull_d_flags -fversion=MULL_ENABLE_MULIB -lpthread -ldl -lm)
            elseif(Armory_TEST_D_COMPILER MATCHES "ldc")
                list(APPEND _mull_d_flags -d-version=MULL_ENABLE_MULIB -L-lpthread -L-ldl -L-lm)
            else()
                list(APPEND _mull_d_flags -version=MULL_ENABLE_MULIB -L-lpthread -L-ldl -L-lm)
            endif()
            list(APPEND _mull_d_flags $<TARGET_FILE:mull-mulib-bridge>)
            set(_mull_d_native_dep mull-mulib-bridge)
        endif()
        add_custom_command(OUTPUT ${_mull_d_test}
            COMMAND ${Armory_TEST_D_COMPILER} ${_mull_d_flags} -I${CMAKE_CURRENT_SOURCE_DIR}/..
                ${CMAKE_CURRENT_SOURCE_DIR}/test_mull.d ${CMAKE_CURRENT_SOURCE_DIR}/../Armory.d ${_mull_d_sources}
            DEPENDS test_mull.d ../Armory.d ${_mull_d_sources} ${_mull_d_native_dep} VERBATIM)
        add_custom_target(mull_d_test_build ALL DEPENDS ${_mull_d_test})
        add_test(NAME mull_host_d COMMAND ${_mull_d_test})
        list(APPEND _mull_parity_args --d ${_mull_d_test})
    endif()
endif()

add_test(NAME mull_conformance COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_mull.py ${_mull_parity_args})
add_test(NAME mull_conformance_unoptimized COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check_mull.py ${_mull_parity_args})
set_tests_properties(mull_conformance_unoptimized PROPERTIES ENVIRONMENT "MULL_NO_OPTIMIZE=1")

option(Armory_TEST_LUA "Test the optional Lua preprocessing bridge" OFF)
if(Armory_TEST_LUA)
    find_package(Lua 5.3 REQUIRED)
    add_executable(armory_lua_tests test_lua_preprocessor.cpp)
    target_compile_definitions(armory_lua_tests PRIVATE ARMORY_ENABLE_LUA)
    target_include_directories(armory_lua_tests PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(armory_lua_tests PRIVATE Armory ${LUA_LIBRARIES})
    add_test(NAME armory_lua_preprocessor COMMAND armory_lua_tests)
endif()

find_package(fmt CONFIG QUIET)
if(TARGET fmt::fmt)
    add_executable(armory_enum_fmt_tests test_enum_format.cpp)
    target_compile_definitions(armory_enum_fmt_tests PRIVATE ARMORY_TEST_FMT)
    target_link_libraries(armory_enum_fmt_tests PRIVATE Armory fmt::fmt)
    add_test(NAME armory_enum_fmt COMMAND armory_enum_fmt_tests)
endif()
