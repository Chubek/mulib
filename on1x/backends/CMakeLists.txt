set(_on1x_backend_artifacts)
foreach(_backend IN ITEMS c11 mir-c11 mir-cfg mir-eval mir-text)
  string(REPLACE "-" "_" _suffix "${_backend}")
  set(_target "on1x_backend_${_suffix}")
  add_library("${_target}" STATIC "${_backend}/Backend.cpp")
  target_compile_features("${_target}" PRIVATE cxx_std_20)
  target_include_directories("${_target}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/src")
  target_link_libraries("${_target}" PRIVATE on1x_frontend On1x::SDK)
  list(APPEND _on1x_backend_artifacts "${_target}")
endforeach()

if(ON1X_BUILD_MIR_ARCHON)
  add_subdirectory(mir-archon)
  if(TARGET on1x_backend_mir_archon)
    list(APPEND _on1x_backend_artifacts on1x_backend_mir_archon)
  endif()
endif()

add_library(on1x_builtin_backends STATIC Builtin.cpp)
target_compile_features(on1x_builtin_backends PRIVATE cxx_std_20)
target_include_directories(on1x_builtin_backends
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/src")
target_link_libraries(on1x_builtin_backends PRIVATE on1x_frontend On1x::SDK ${_on1x_backend_artifacts})
if(TARGET on1x_backend_mir_archon)
  target_compile_definitions(on1x_builtin_backends PRIVATE ON1X_WITH_MIR_ARCHON=1)
endif()

include(link/On1xBackendLink.cmake)
on1x_link_backend_artifacts(on1x_builtin_backends)

if(TARGET on1x_backend_mir_archon AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING
    AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
    AND ON1X_ARCHON_TARGET_TRIPLE STREQUAL "x86_64-unknown-linux-gnu")
  add_executable(on1x_archon_driver_test mir-archon/tests/TestDriver.cpp)
  target_link_libraries(on1x_archon_driver_test PRIVATE on1x_builtin_backends on1x_frontend)
  add_test(NAME on1x.backends.mir_archon_driver COMMAND on1x_archon_driver_test)
  add_test(NAME on1x.backends.mir_archon_native_build
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/mir-archon/tests/test_native_driver.py"
      --build-dir "${CMAKE_BINARY_DIR}" --cc "${CMAKE_C_COMPILER}" --sdk)
  set_tests_properties(on1x.backends.mir_archon_native_build PROPERTIES TIMEOUT 240)
endif()

if(BUILD_TESTING)
  foreach(_backend IN ITEMS c11 mir_c11)
    if(_backend STREQUAL "c11")
      set(_test c11/TestBackend.cpp)
    else()
      set(_test mir-c11/test_backend.cpp)
    endif()
    add_executable(on1x_backend_test_${_backend} "${_test}")
    target_include_directories(on1x_backend_test_${_backend} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/src")
    target_link_libraries(on1x_backend_test_${_backend} PRIVATE
      on1x_backend_${_backend} on1x_frontend On1x::SDK)
    add_test(NAME on1x.backends.${_backend} COMMAND on1x_backend_test_${_backend})
  endforeach()
endif()
