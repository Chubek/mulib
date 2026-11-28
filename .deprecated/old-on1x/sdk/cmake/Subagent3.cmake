# Subagent 3: canonical type, semantic, HIR and MIR SDK adapters.

target_sources(on1x_sdk PRIVATE
    src/ir/Common.cpp
    src/ir/HIR.cpp
    src/ir/MIR.cpp
    src/ir/Semantics.cpp
    src/ir/Serialization.cpp
    src/ir/Transaction.cpp
    src/types/Type.cpp
)
target_include_directories(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

include("${CMAKE_CURRENT_LIST_DIR}/Integration3.cmake")

if(BUILD_TESTING)
    add_executable(on1x_sdk_test_semantics_ir tests/semantics_ir.cpp)
    target_link_libraries(on1x_sdk_test_semantics_ir
        PRIVATE On1x::SDKSemanticsIR)
    target_include_directories(on1x_sdk_test_semantics_ir
        PRIVATE src/ir)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_semantics_ir
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.semantics_ir
        COMMAND on1x_sdk_test_semantics_ir)

    add_executable(on1x_sdk_test_semantics_ir_c tests/semantics_ir_c.c)
    target_link_libraries(on1x_sdk_test_semantics_ir_c
        PRIVATE On1x::SDKSemanticsIR)
    set_target_properties(on1x_sdk_test_semantics_ir_c PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_semantics_ir_c
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.semantics_ir_c
        COMMAND on1x_sdk_test_semantics_ir_c)

    set(_on1x_sdk_semantics_headers
        On1xSDK-SemanticTypes.h
        On1xSDK-Type.h
        On1xSDK-HIR.h
        On1xSDK-MIR.h
        On1xSDK-IRSerialize.h
        On1xSDK-IRTransaction.h
        On1xSDK-Semantics.h
    )
    foreach(_header IN LISTS _on1x_sdk_semantics_headers)
        string(MAKE_C_IDENTIFIER "${_header}" _header_id)
        add_executable("on1x_sdk_header_${_header_id}_c"
            tests/header_standalone.c)
        target_include_directories("on1x_sdk_header_${_header_id}_c"
            PRIVATE include)
        target_compile_definitions("on1x_sdk_header_${_header_id}_c"
            PRIVATE "ON1X_SDK_TEST_HEADER=\"${_header}\"")
        set_target_properties("on1x_sdk_header_${_header_id}_c" PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options("on1x_sdk_header_${_header_id}_c"
                PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME "on1x.sdk.header.${_header_id}.c"
            COMMAND "on1x_sdk_header_${_header_id}_c")

        add_executable("on1x_sdk_header_${_header_id}_cxx17"
            tests/header_standalone.cpp)
        target_include_directories("on1x_sdk_header_${_header_id}_cxx17"
            PRIVATE include)
        target_compile_definitions("on1x_sdk_header_${_header_id}_cxx17"
            PRIVATE "ON1X_SDK_TEST_HEADER=\"${_header}\"")
        set_target_properties("on1x_sdk_header_${_header_id}_cxx17"
            PROPERTIES
                CXX_STANDARD 17
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options("on1x_sdk_header_${_header_id}_cxx17"
                PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME "on1x.sdk.header.${_header_id}.cxx17"
            COMMAND "on1x_sdk_header_${_header_id}_cxx17")
    endforeach()
endif()
