# Opt-in subdirectory. Include from on1x/backends after an installed,
# target-pinned ArchonBackend package is available.
cmake_minimum_required(VERSION 3.20)
find_package(ArchonBackend CONFIG REQUIRED)
if(NOT TARGET on1x_frontend)
  message(FATAL_ERROR "mir-archon must be added by On1x after on1x_frontend")
endif()

add_library(on1x_archon_bridge STATIC Bridge.cpp)
target_compile_features(on1x_archon_bridge PUBLIC cxx_std_20)
target_include_directories(on1x_archon_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(on1x_archon_bridge PUBLIC on1x_frontend Archon::Backend)

# Link on1x_archon_scalar_abi into the generated executable (not the compiler).
# Its symbol names provide the exact scalar/float bit-pattern ABI contract.
add_library(on1x_archon_scalar_abi STATIC ScalarABI.c)
target_compile_features(on1x_archon_scalar_abi PRIVATE c_std_11)
target_link_libraries(on1x_archon_scalar_abi PUBLIC on1x_runtime)

add_executable(on1x-archon-mir Tool.cpp)
target_link_libraries(on1x-archon-mir PRIVATE on1x_archon_bridge)

option(ON1X_ARCHON_ENABLE_SDK_ADAPTER "Build target-aware trusted SDK 2 provider" OFF)
set(ON1X_ARCHON_TARGET_TRIPLE "" CACHE STRING
    "Exact On1x SDK target triple corresponding to generated Archon::Backend")
if(ON1X_ARCHON_ENABLE_SDK_ADAPTER)
  if(NOT TARGET On1x::SDK)
    message(FATAL_ERROR "mir-archon SDK adapter requires On1x::SDK")
  endif()
  if(NOT ON1X_ARCHON_TARGET_TRIPLE)
    message(FATAL_ERROR "Set ON1X_ARCHON_TARGET_TRIPLE for native SDK admission")
  endif()
  add_library(on1x_backend_mir_archon STATIC Backend.cpp)
  target_include_directories(on1x_backend_mir_archon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/src")
  target_compile_features(on1x_backend_mir_archon PRIVATE cxx_std_20)
  target_compile_definitions(on1x_backend_mir_archon PRIVATE
    ON1X_ARCHON_TARGET_TRIPLE="${ON1X_ARCHON_TARGET_TRIPLE}")
  target_link_libraries(on1x_backend_mir_archon PRIVATE
    on1x_archon_bridge On1x::SDK)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_executable(on1x_archon_test tests/TestBridge.cpp)
  target_link_libraries(on1x_archon_test PRIVATE on1x_archon_bridge)
  add_test(NAME on1x.backends.mir_archon COMMAND on1x_archon_test)
  add_executable(on1x_archon_tuple_phi_test tests/TestTuplePhi.cpp)
  target_link_libraries(on1x_archon_tuple_phi_test PRIVATE on1x_archon_bridge)
  add_test(NAME on1x.backends.mir_archon_tuple_phi COMMAND on1x_archon_tuple_phi_test)
  if(Python3_EXECUTABLE)
    add_test(NAME on1x.backends.mir_archon_scalar_abi_regen
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_scalar_abi.py" --check)
  endif()
endif()
