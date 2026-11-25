# Project/tooling/bindings targets owned by SDK 2 subagent 7.

add_library(on1x_sdk_project SHARED
    src/project/Project.cpp
    src/project/Manifest.cpp
    src/project/BuildGraph.cpp
    src/project/PackageResolver.cpp
    src/project/Process.cpp
    src/project/Command.cpp
    src/project/Tooling.cpp
)
add_library(On1x::Project ALIAS on1x_sdk_project)
set_target_properties(on1x_sdk_project PROPERTIES
    EXPORT_NAME Project
    OUTPUT_NAME On1xSDKProject
    VERSION 2.0.0
    SOVERSION 2
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
target_compile_features(on1x_sdk_project PRIVATE cxx_std_20)
target_compile_definitions(on1x_sdk_project PRIVATE
    ON1X_SDK_PROJECT_BUILD=1)
target_include_directories(on1x_sdk_project PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/on1x-sdk>
)
target_link_libraries(on1x_sdk_project PUBLIC On1x::SDK)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_sdk_project PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
if(UNIX AND NOT APPLE)
    set(_on1x_sdk_project_map
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/On1xSDKProject.map")
    target_link_options(on1x_sdk_project PRIVATE
        "LINKER:--version-script=${_on1x_sdk_project_map}")
    set_property(TARGET on1x_sdk_project APPEND PROPERTY
        LINK_DEPENDS "${_on1x_sdk_project_map}")
endif()

install(TARGETS on1x_sdk_project
    EXPORT On1xSDKTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT On1xSDK
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT On1xSDK
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT On1xSDK
)
install(PROGRAMS tools/on1x-sdk
    DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT On1xSDK)
install(FILES tools/README.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/tools" COMPONENT On1xSDK)
install(FILES bindings/python/on1x_sdk.py
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/on1x-sdk/python"
    COMPONENT On1xSDK)
install(FILES bindings/README.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/bindings" COMPONENT On1xSDK)
# The installed SDK utility reuses the canonical bounded project/lock parser.
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/on1x-project.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate-diagnostic-catalog.py"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/on1x/tools"
    COMPONENT On1xSDK)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../tools/on1x_optimization.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/on1x_toolchain.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/on1x_dotenv.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/on1x_proxy.py"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/on1x/tools" COMPONENT On1xSDK)

if(BUILD_TESTING)
    add_executable(on1x_sdk_test_project tests/project.cpp)
    target_link_libraries(on1x_sdk_test_project PRIVATE On1x::Project)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_project PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.project COMMAND on1x_sdk_test_project)

    add_executable(on1x_sdk_test_project_c tests/project_c.c)
    target_link_libraries(on1x_sdk_test_project_c PRIVATE On1x::Project)
    set_target_properties(on1x_sdk_test_project_c PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(on1x_sdk_test_project_c PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME on1x.sdk.project_c COMMAND on1x_sdk_test_project_c)
endif()
