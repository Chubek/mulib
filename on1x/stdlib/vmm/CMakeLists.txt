if(NOT TARGET On1xArchVirt::Library)
  message(FATAL_ERROR "The std.vmm native companion requires On1xArchVirt::Library")
endif()

add_library(on1x_stdlib_vmm STATIC vmm.cpp)
target_compile_features(on1x_stdlib_vmm PRIVATE cxx_std_17)
target_include_directories(on1x_stdlib_vmm PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:include/on1x>")
target_link_libraries(on1x_stdlib_vmm PUBLIC On1xArchVirt::Library)
install(TARGETS on1x_stdlib_vmm ARCHIVE DESTINATION lib)
install(FILES vmm.h DESTINATION include/on1x)

if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(on1x_stdlib_vmm_test tests/bridge.c)
  target_compile_features(on1x_stdlib_vmm_test PRIVATE c_std_11)
  target_link_libraries(on1x_stdlib_vmm_test PRIVATE on1x_stdlib_vmm Threads::Threads)
  add_test(NAME on1x.stdlib.vmm.native
    COMMAND on1x_stdlib_vmm_test "${ON1X_ARCHVIRT_ROOT}/share/archvirt")
  add_test(NAME on1x.stdlib.vmm.config
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/config.py"
      "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}")
  if(ON1X_WITH_EXOML_EXTRACT AND ON1X_WITH_CHISEL)
    add_test(NAME on1x.stdlib.vmm.admission
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/admission.py"
        "$<TARGET_FILE:on1x-bind>" "$<TARGET_FILE:on1xc>" "${CMAKE_C_COMPILER}"
        "${ON1X_ARCHVIRT_ROOT}/include/archvirt.h" "$<TARGET_FILE:On1xArchVirt::Library>")
    set_tests_properties(on1x.stdlib.vmm.admission PROPERTIES TIMEOUT 90)
  endif()
  set_tests_properties(on1x.stdlib.vmm.native on1x.stdlib.vmm.config PROPERTIES TIMEOUT 90)
endif()
