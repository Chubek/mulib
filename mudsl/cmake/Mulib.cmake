find_path(MULL_MULIB_INCLUDE_DIR mulib-parsec.h
    HINTS "${CMAKE_CURRENT_LIST_DIR}/../../include"
    DOC "MuLIB public header directory")
set(_mull_mulib_default OFF)
if(MULL_MULIB_INCLUDE_DIR)
    set(_mull_mulib_default ON)
endif()
option(MULL_ENABLE_MULIB "Build the complete MuLL MuLIB wrapper" ${_mull_mulib_default})
if(MULL_ENABLE_MULIB)
    if(NOT MULL_MULIB_INCLUDE_DIR)
        message(FATAL_ERROR "Set MULL_MULIB_INCLUDE_DIR to the MuLIB include directory")
    endif()
    enable_language(C)
    find_package(Threads REQUIRED)
    add_library(mull-mulib-bridge STATIC
        ${CMAKE_CURRENT_LIST_DIR}/../mull/stdlib/Mulib/Bridge.c
        ${CMAKE_CURRENT_LIST_DIR}/../mull/stdlib/Mulib/Implementation.c)
    add_library(MuDSL::Mulib ALIAS mull-mulib-bridge)
    set_target_properties(mull-mulib-bridge PROPERTIES EXPORT_NAME Mulib
        C_STANDARD 11 C_STANDARD_REQUIRED YES POSITION_INDEPENDENT_CODE YES)
    target_include_directories(mull-mulib-bridge PRIVATE ${MULL_MULIB_INCLUDE_DIR})
    target_include_directories(mull-mulib-bridge INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(mull-mulib-bridge INTERFACE MULL_ENABLE_MULIB)
    target_compile_features(mull-mulib-bridge INTERFACE cxx_std_20)
    target_link_libraries(mull-mulib-bridge PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
