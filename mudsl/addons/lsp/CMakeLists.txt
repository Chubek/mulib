cmake_minimum_required(VERSION 3.24)
project(MuLLEditorChecker LANGUAGES CXX)
add_executable(mull-check check.cpp)
target_compile_features(mull-check PRIVATE cxx_std_20)
target_include_directories(mull-check PRIVATE ../..)
install(TARGETS mull-check RUNTIME DESTINATION bin)
include(CTest)
if(BUILD_TESTING)
    find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
    add_test(NAME mu_editor_addons COMMAND ${CMAKE_COMMAND} -E env
        "MULL_CHECKER=$<TARGET_FILE:mull-check>"
        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_addons.py)
endif()
