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

add_library(on1x_builtin_backends STATIC Builtin.cpp)
target_compile_features(on1x_builtin_backends PRIVATE cxx_std_20)
target_include_directories(on1x_builtin_backends
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/src")
target_link_libraries(on1x_builtin_backends PRIVATE on1x_frontend On1x::SDK ${_on1x_backend_artifacts})

include(link/On1xBackendLink.cmake)
on1x_link_backend_artifacts(on1x_builtin_backends)

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
