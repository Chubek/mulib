# The canonical MIR analysis implementation belongs to the compiler library.
target_sources(on1x_frontend PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../src/analyzer/mir_analysis.cpp"
)
# In-tree SDK consumers get the canonical MIR implementation transitively.
# Installed aggregate packaging is principal-owned and is recorded in report 4.
target_link_libraries(on1x_sdk PRIVATE
  "$<BUILD_INTERFACE:on1x_frontend>"
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../../src/analyzer/mir_analysis.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
  )
endif()

if(TARGET on1x_novostone)
  target_sources(on1x_sdk PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../src/analysis/NovostoneAdapter.cpp")
  target_compile_definitions(on1x_sdk PRIVATE ON1X_SDK_WITH_NOVOSTONE=1)
  target_link_libraries(on1x_sdk PRIVATE
    "$<BUILD_INTERFACE:on1x_novostone>")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
      "${CMAKE_CURRENT_LIST_DIR}/../src/analysis/NovostoneAdapter.cpp"
      PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
    )
  endif()
endif()

if(BUILD_TESTING)
  add_executable(on1x_sdk_test_analysis_passes
    "${CMAKE_CURRENT_LIST_DIR}/../tests/analysis_passes.cpp")
  target_include_directories(on1x_sdk_test_analysis_passes PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../src")
  target_link_libraries(on1x_sdk_test_analysis_passes PRIVATE
    On1x::SDK on1x_frontend)
  if(TARGET on1x_novostone)
    target_compile_definitions(on1x_sdk_test_analysis_passes PRIVATE
      ON1X_SDK_TEST_WITH_NOVOSTONE=1)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_sdk_test_analysis_passes PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME on1x.sdk.analysis_passes
    COMMAND on1x_sdk_test_analysis_passes)
  set_tests_properties(on1x.sdk.analysis_passes PROPERTIES TIMEOUT 60)

  add_executable(on1x_sdk_test_analysis_c_abi
    "${CMAKE_CURRENT_LIST_DIR}/../tests/analysis_c_abi.c")
  target_link_libraries(on1x_sdk_test_analysis_c_abi PRIVATE
    On1x::SDK on1x_frontend)
  set_target_properties(on1x_sdk_test_analysis_c_abi PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    LINKER_LANGUAGE CXX)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_sdk_test_analysis_c_abi PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME on1x.sdk.analysis_c_abi
    COMMAND on1x_sdk_test_analysis_c_abi)

  if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME on1x.sdk.analysis_passes_c11
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/../tests/analysis_passes_c11.py"
        "$<TARGET_FILE:on1x_sdk_test_analysis_passes>"
        "${CMAKE_C_COMPILER}"
        "${CMAKE_CURRENT_LIST_DIR}/../../runtime"
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/sdk-analysis-passes-c11")
    set_tests_properties(on1x.sdk.analysis_passes_c11 PROPERTIES TIMEOUT 90)
  endif()

  set(_on1x_sdk4_headers
    On1xSDK-Query.h
    On1xSDK-AnalysisManager.h
    On1xSDK-CFG.h
    On1xSDK-Dominator.h
    On1xSDK-Loop.h
    On1xSDK-Liveness.h
    On1xSDK-EffectAnalysis.h
    On1xSDK-Pass.h
    On1xSDK-Pipeline.h
    On1xSDK-PassInstrumentation.h
    On1xSDK-PassManager.h
    On1xSDK-TranslationValidation.h
  )
  foreach(_header IN LISTS _on1x_sdk4_headers)
    string(MAKE_C_IDENTIFIER "${_header}" _header_id)
    set(_c_source "${CMAKE_CURRENT_BINARY_DIR}/sdk4-${_header_id}.c")
    set(_cxx_source "${CMAKE_CURRENT_BINARY_DIR}/sdk4-${_header_id}.cpp")
    file(WRITE "${_c_source}" "#include <${_header}>\nint sdk4_header_c(void) { return 0; }\n")
    file(WRITE "${_cxx_source}" "#include <${_header}>\nint sdk4_header_cxx() { return 0; }\n")
    add_library("on1x_sdk4_header_c_${_header_id}" OBJECT "${_c_source}")
    add_library("on1x_sdk4_header_cxx_${_header_id}" OBJECT "${_cxx_source}")
    set_target_properties("on1x_sdk4_header_c_${_header_id}" PROPERTIES
      C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    set_target_properties("on1x_sdk4_header_cxx_${_header_id}" PROPERTIES
      CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    list(APPEND _on1x_sdk4_header_targets
      "on1x_sdk4_header_c_${_header_id}"
      "on1x_sdk4_header_cxx_${_header_id}")
    target_include_directories("on1x_sdk4_header_c_${_header_id}" PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_include_directories("on1x_sdk4_header_cxx_${_header_id}" PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/../include")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options("on1x_sdk4_header_c_${_header_id}" PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options("on1x_sdk4_header_cxx_${_header_id}" PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    endif()
  endforeach()
  add_custom_target(on1x_sdk4_headers DEPENDS ${_on1x_sdk4_header_targets})
endif()
