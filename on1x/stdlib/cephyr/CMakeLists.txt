# This subdirectory is selected only by ON1X_WITH_CEPHYR.
add_library(on1x_stdlib_cephyr STATIC runtime.c)
target_compile_features(on1x_stdlib_cephyr PRIVATE c_std_11)
target_include_directories(on1x_stdlib_cephyr PRIVATE ../../runtime)
# Migrate the old source-tree default in already-configured builds; retain
# explicitly selected deployment paths.
if(ON1X_STDLIB_CEPHYR_CONFIG STREQUAL "${PROJECT_SOURCE_DIR}/src/backend/cephyr-baseline.toml")
  unset(ON1X_STDLIB_CEPHYR_CONFIG CACHE)
endif()
set(ON1X_STDLIB_CEPHYR_CONFIG "${cephyr_config_dir}/cephyr-baseline.toml"
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
  add_test(NAME on1x.stdlib.cephyr.preprocess COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess.py"
    "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" "$<TARGET_FILE:on1x_stdlib_cephyr>"
    "$<TARGET_FILE:Cephyr::Cephyr>")
  add_test(NAME on1x.stdlib.cephyr.documents COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/documents.py"
    "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}" "$<TARGET_FILE:on1x_stdlib_cephyr>"
    "$<TARGET_FILE:Cephyr::Cephyr>")
  set_tests_properties(on1x.stdlib.cephyr on1x.stdlib.cephyr.preprocess
    on1x.stdlib.cephyr.documents PROPERTIES TIMEOUT 120)
endif()
