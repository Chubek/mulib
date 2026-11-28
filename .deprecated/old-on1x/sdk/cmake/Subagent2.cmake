# Compiler/frontend SDK implementation. The canonical frontend target is
# declared later by the parent project and linked in Integration2.cmake.
target_sources(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/Compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/Compilation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Frontend.cpp"
)
target_include_directories(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compiler"
)
