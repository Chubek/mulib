if(BUILD_TESTING)
  set(_on1x_sdk5_root "${CMAKE_CURRENT_LIST_DIR}/..")

  add_executable(on1x_sdk_test_target_v2
    "${_on1x_sdk5_root}/tests/target_v2.cpp")
  target_link_libraries(on1x_sdk_test_target_v2 PRIVATE On1x::SDK)
  add_test(NAME on1x.sdk.target_v2 COMMAND on1x_sdk_test_target_v2)

  add_executable(on1x_sdk_test_backend_v2
    "${_on1x_sdk5_root}/tests/backend_v2.cpp")
  target_include_directories(on1x_sdk_test_backend_v2 PRIVATE
    "${_on1x_sdk5_root}/src")
  target_link_libraries(on1x_sdk_test_backend_v2 PRIVATE On1x::SDK)
  add_test(NAME on1x.sdk.backend_v2 COMMAND on1x_sdk_test_backend_v2)

  add_executable(on1x_sdk_test_toolchain_v2
    "${_on1x_sdk5_root}/tests/toolchain_v2.cpp")
  target_link_libraries(on1x_sdk_test_toolchain_v2 PRIVATE On1x::SDK)
  add_test(NAME on1x.sdk.toolchain_v2 COMMAND on1x_sdk_test_toolchain_v2)

  add_executable(on1x_sdk_test_external_backend
    "${_on1x_sdk5_root}/tests/external_backend.cpp"
    "${_on1x_sdk5_root}/examples/backend/external_backend.c")
  target_include_directories(on1x_sdk_test_external_backend PRIVATE
    "${_on1x_sdk5_root}/src"
    "${_on1x_sdk5_root}/examples/backend")
  target_link_libraries(on1x_sdk_test_external_backend PRIVATE On1x::SDK)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
      "${_on1x_sdk5_root}/examples/backend/external_backend.c"
      PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror")
  endif()
  add_test(NAME on1x.sdk.external_backend COMMAND on1x_sdk_test_external_backend)

  add_executable(on1x_sdk_test_builtin_providers_v2
    "${_on1x_sdk5_root}/tests/builtin_providers_v2.cpp")
  target_include_directories(on1x_sdk_test_builtin_providers_v2 PRIVATE
    "${_on1x_sdk5_root}/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../backends")
  target_link_libraries(on1x_sdk_test_builtin_providers_v2 PRIVATE
    on1x_builtin_backends on1x_frontend On1x::SDK)
  add_test(NAME on1x.sdk.builtin_providers_v2
    COMMAND on1x_sdk_test_builtin_providers_v2)

  add_executable(on1x_sdk_test_backend_c_abi
    "${_on1x_sdk5_root}/tests/backend_c_abi.c")
  target_link_libraries(on1x_sdk_test_backend_c_abi PRIVATE On1x::SDK)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(on1x_sdk_test_backend_c_abi PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME on1x.sdk.backend_c_abi COMMAND on1x_sdk_test_backend_c_abi)

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND
     CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME on1x.sdk.backend_headers
      COMMAND "${Python3_EXECUTABLE}"
        "${_on1x_sdk5_root}/tests/check_backend_headers.py"
        "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
        "${_on1x_sdk5_root}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk5-headers"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk5-headers.stamp")
    set_tests_properties(on1x.sdk.backend_headers PROPERTIES TIMEOUT 120)
  endif()

  if(UNIX AND NOT CMAKE_CROSSCOMPILING AND
     CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME on1x.sdk.backend_relocated_consumer
      COMMAND "${Python3_EXECUTABLE}"
        "${_on1x_sdk5_root}/tests/check_backend_package.py"
        "${CMAKE_COMMAND}" "${CMAKE_CURRENT_BINARY_DIR}" "$<CONFIG>"
        "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
        "${CMAKE_C_FLAGS}" "${CMAKE_CXX_FLAGS}" "${CMAKE_EXE_LINKER_FLAGS}")
    set_tests_properties(on1x.sdk.backend_relocated_consumer PROPERTIES TIMEOUT 120)
  endif()

  add_executable(on1x_sdk_backend_artifact_emit
    "${_on1x_sdk5_root}/tests/backend_artifact_emit.cpp")
  target_include_directories(on1x_sdk_backend_artifact_emit PRIVATE
    "${_on1x_sdk5_root}/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../backends")
  target_link_libraries(on1x_sdk_backend_artifact_emit PRIVATE
    on1x_backend_mir_c11 on1x_frontend On1x::SDK)
  if(UNIX AND NOT CMAKE_CROSSCOMPILING AND
     CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME on1x.sdk.backend_artifact
      COMMAND "${Python3_EXECUTABLE}"
        "${_on1x_sdk5_root}/tests/check_backend_artifact.py"
        "$<TARGET_FILE:on1x_sdk_backend_artifact_emit>"
        "${CMAKE_C_COMPILER}"
        "${CMAKE_CURRENT_LIST_DIR}/../.."
        "${CMAKE_CURRENT_BINARY_DIR}/sdk5-artifact")
    set_tests_properties(on1x.sdk.backend_artifact PROPERTIES TIMEOUT 120)
  endif()
endif()
