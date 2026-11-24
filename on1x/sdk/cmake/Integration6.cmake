set(_on1x_ffi_sdk "${CMAKE_CURRENT_LIST_DIR}/..")
target_sources(on1x_sdk PRIVATE "${_on1x_ffi_sdk}/src/contracts/FFIDescriptor.cpp")
if(BUILD_TESTING)
    add_executable(on1x_sdk_ffi_descriptor_c
        "${_on1x_ffi_sdk}/tests/contracts/ffi_descriptor.c")
    target_link_libraries(on1x_sdk_ffi_descriptor_c PRIVATE On1x::SDK)
    set_target_properties(on1x_sdk_ffi_descriptor_c PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME on1x.sdk6.ffi_descriptor_not_built COMMAND on1x_sdk_ffi_descriptor_c)
endif()
