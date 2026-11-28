# Late integration tests; Python and all canonical targets are available here.

if(BUILD_TESTING)
    add_executable(on1x_sdk_mir_directive_tests
        "${CMAKE_CURRENT_LIST_DIR}/../tests/mir_directives.cpp")
    target_include_directories(on1x_sdk_mir_directive_tests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../src")
    target_link_libraries(on1x_sdk_mir_directive_tests PRIVATE
        on1x_builtin_backends On1x::SDKSemanticsIR)
    if(TARGET on1x_backend_mir_archon AND
       ON1X_ARCHON_TARGET_TRIPLE STREQUAL "x86_64-unknown-linux-gnu")
        target_compile_definitions(on1x_sdk_mir_directive_tests PRIVATE ON1X_TEST_MIR_ARCHON=1)
    endif()
    add_test(NAME on1x.sdk.mir_directives COMMAND on1x_sdk_mir_directive_tests)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME on1x.sdk.project_headers
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_project_headers.py"
            "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
            "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(on1x.sdk.project_headers PROPERTIES TIMEOUT 60)

    if(UNIX AND NOT APPLE AND CMAKE_NM)
        add_test(NAME on1x.sdk.project_exports
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_project_exports.py"
                "${CMAKE_NM}" "$<TARGET_FILE:on1x_sdk_project>")
        set_tests_properties(on1x.sdk.project_exports PROPERTIES TIMEOUT 60)
    endif()

    add_test(NAME on1x.sdk.python_binding
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/test_python_binding.py"
            "$<TARGET_FILE:on1x_sdk_project>"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/bindings/python")
    set_tests_properties(on1x.sdk.python_binding PROPERTIES TIMEOUT 60)

    add_test(NAME on1x.sdk.tool
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/test_sdk_tool.py"
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tools/on1x-sdk"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(on1x.sdk.tool PROPERTIES TIMEOUT 90)

    add_test(NAME on1x.sdk.project_installed_consumer
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/tests/check_project_package.py"
            "${CMAKE_COMMAND}" "${CMAKE_CURRENT_BINARY_DIR}" "$<CONFIG>"
            "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_FLAGS}"
            "${CMAKE_EXE_LINKER_FLAGS}" "${CMAKE_INSTALL_LIBDIR}"
            "${CMAKE_INSTALL_BINDIR}" "${Python3_EXECUTABLE}")
    set_tests_properties(on1x.sdk.project_installed_consumer
        PROPERTIES TIMEOUT 120)
endif()
