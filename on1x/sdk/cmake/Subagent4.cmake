# Analysis/transformation ABI implementation. Canonical compiler linkage is
# supplied by Integration4.cmake after on1x_frontend has been declared.
target_sources(on1x_sdk PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../src/analysis/AnalysisManager.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/transformation/PassManager.cpp"
)
target_include_directories(on1x_sdk PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../src/analysis/AnalysisManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/transformation/PassManager.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
  )
endif()
