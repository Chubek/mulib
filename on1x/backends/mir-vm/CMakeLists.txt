# VMWeave is a build-time generator. Its compiled C dispatcher has no Lua,
# Kaguya, VMWeave toolchain, C compiler, or dynamic compilation dependency.
set(ON1X_VMWEAVE_EXECUTABLE "" CACHE FILEPATH "Pinned LibStone vmweave-cli; regenerate the On1x VM when set")
set(_vm_generated "${CMAKE_CURRENT_SOURCE_DIR}/snapshot")
set(_vm_files On1xVM.c On1xVM.h On1xVM.dispatch On1xVM.frame On1xVM.insncode
              On1xVM.opcode On1xVM.stk00 On1xVM.tape On1xVM.token)
if(ON1X_VMWEAVE_EXECUTABLE)
  set(_vm_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(_vm_outputs ${_vm_files})
  list(TRANSFORM _vm_outputs PREPEND "${_vm_generated}/")
  add_custom_command(OUTPUT ${_vm_outputs}
    COMMAND "${ON1X_VMWEAVE_EXECUTABLE}" --generate -o "${_vm_generated}" On1xVM.lua
    DEPENDS On1xVM.lua "${ON1X_VMWEAVE_EXECUTABLE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
else()
  include("${CMAKE_CURRENT_SOURCE_DIR}/snapshot.cmake")
endif()
add_library(on1x_mir_vm STATIC vm.cpp wire.cpp "${_vm_generated}/On1xVM.c")
target_include_directories(on1x_mir_vm PRIVATE "${_vm_generated}" "${CMAKE_CURRENT_SOURCE_DIR}/../../src/backend")
target_link_libraries(on1x_mir_vm PUBLIC on1x_frontend)
set_target_properties(on1x_mir_vm PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_executable(on1x-vm main.cpp)
target_link_libraries(on1x-vm PRIVATE on1x_mir_vm)
install(TARGETS on1x-vm RUNTIME DESTINATION bin)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_executable(on1x_vm_tests tests/vm.cpp)
  target_include_directories(on1x_vm_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/backend")
  target_link_libraries(on1x_vm_tests PRIVATE on1x_mir_vm)
  add_test(NAME on1x.vm.opcodes COMMAND on1x_vm_tests)
  add_test(NAME on1x.vm.source COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/source.py" "$<TARGET_FILE:on1xc>" "$<TARGET_FILE:on1x-vm>"
    "${CMAKE_C_COMPILER}" "${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts")
  if(ON1X_VMWEAVE_EXECUTABLE)
    add_test(NAME on1x.vm.regeneration COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/regen.py" "${ON1X_VMWEAVE_EXECUTABLE}" --check)
  endif()
endif()
