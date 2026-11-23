# This subdirectory is selected only by ON1X_WITH_CEPHYR.
add_library(on1x_stdlib_cephyr STATIC runtime.c)
target_compile_features(on1x_stdlib_cephyr PRIVATE c_std_11)
target_include_directories(on1x_stdlib_cephyr PRIVATE ../../runtime)
set(ON1X_STDLIB_CEPHYR_CONFIG "${PROJECT_SOURCE_DIR}/src/backend/cephyr-baseline.toml"
  CACHE FILEPATH "Absolute trusted Cephyr baseline TOML path on the consumer host")
if(NOT IS_ABSOLUTE "${ON1X_STDLIB_CEPHYR_CONFIG}")
  message(FATAL_ERROR "ON1X_STDLIB_CEPHYR_CONFIG must be an absolute path")
endif()
target_compile_definitions(on1x_stdlib_cephyr PRIVATE
  ON1X_CEPHYR_CONFIG="${ON1X_STDLIB_CEPHYR_CONFIG}"
  ON1X_CEPHYR_RESOURCE_ROOT="${ON1X_CEPHYR_RESOURCE_ROOT}"
  ON1X_CEPHYR_ARCHON_ROOT="${ON1X_CEPHYR_ARCHON_ROOT}")
target_link_libraries(on1x_stdlib_cephyr PUBLIC Cephyr::Cephyr)
install(TARGETS on1x_stdlib_cephyr ARCHIVE DESTINATION lib)
if(BUILD_TESTING)
  add_test(NAME on1x.stdlib.cephyr COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_stdlib_cephyr.py"
    "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" "$<TARGET_FILE:on1x_stdlib_cephyr>"
    "$<TARGET_FILE:Cephyr::Cephyr>" "$<BOOL:${ON1X_WITH_CHISEL}>")
  set_tests_properties(on1x.stdlib.cephyr PROPERTIES TIMEOUT 120)
endif()
