cmake_minimum_required(VERSION 3.24)
project(MuDSL VERSION 1.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

add_library(MuDSL INTERFACE)
add_library(MuDSL::MuDSL ALIAS MuDSL)
target_include_directories(MuDSL INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(MuDSL INTERFACE cxx_std_20)
include(cmake/Mulib.cmake)
if(MULL_ENABLE_MULIB)
    target_link_libraries(MuDSL INTERFACE mull-mulib-bridge)
    install(TARGETS mull-mulib-bridge EXPORT MuDSLTargets)
endif()

option(BUILD_EXAMPLES "Build the MuDSL and MuLL examples" OFF)
if(BUILD_EXAMPLES)
    file(GLOB examples CONFIGURE_DEPENDS examples/*.cpp)
    foreach(source IN LISTS examples)
        get_filename_component(name ${source} NAME_WE)
        add_executable(mudsl_${name} ${source})
        target_link_libraries(mudsl_${name} PRIVATE MuDSL)
    endforeach()
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
add_subdirectory(mull)

install(FILES MuDSL.hpp MuDSL.d DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS MuDSL EXPORT MuDSLTargets)
install(EXPORT MuDSLTargets FILE MuDSLTargets.cmake NAMESPACE MuDSL::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MuDSL)
configure_package_config_file(cmake/MuDSLConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MuDSLConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MuDSL)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/MuDSLConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MuDSLConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MuDSLConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MuDSL)
configure_file(cmake/MuDSL.pc.in ${CMAKE_CURRENT_BINARY_DIR}/MuDSL.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MuDSL.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY mull/stdlib/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mull/stdlib)
install(DIRECTORY mull/stdlib/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mull/stdlib)
install(DIRECTORY mull/native mull/bridge mull/mullier mull/mu3ac DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mull)
install(DIRECTORY mull/mu3ac/rewrite/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mull/mu3ac/rewrite)
install(DIRECTORY mull/rewrite/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mull/rewrite)
install(DIRECTORY book/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/book
    PATTERN build EXCLUDE PATTERN __pycache__ EXCLUDE PATTERN "_minted*" EXCLUDE PATTERN .gitignore EXCLUDE)
install(DIRECTORY manual/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/manual)
install(FILES MuDSL.hpp MuDSL.d DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../MANIFEST.yaml")
    install(FILES ../MANIFEST.yaml DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/reference)
    install(DIRECTORY ../manual/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/reference/manual)
endif()
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL)
install(FILES mull/README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/mull)
install(FILES mull/stdlib/README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MuDSL/mull/stdlib)
