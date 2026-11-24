# Test-only cross-layer qualification. On1x::RT never links the compiler or
# runs the planner, parser, or a generator when loading/invoking an artifact.
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND _on1x_ffi_sdk_typed AND
   ON1X_FFI_TARGET STREQUAL "x86_64-linux-gnu" AND
   EXISTS "${ON1X_FFI_CATALOG}/clamp.ffi" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/on1x_ffi_native.py")
    set(_rt_ffi_tests "${CMAKE_CURRENT_LIST_DIR}/../tests")
    set(_rt_ffi_modes static)
    if(ON1X_RT_BUILD_SHARED AND TARGET on1x_rt_test_dynamic)
        list(APPEND _rt_ffi_modes dynamic)
    endif()
    foreach(_mode IN LISTS _rt_ffi_modes)
        add_test(NAME on1x.runtime.ffi_embedding_${_mode}
            COMMAND "${Python3_EXECUTABLE}" -B "${_rt_ffi_tests}/ffi_embedding.py"
                "${_mode}" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_C_COMPILER}"
                "$<TARGET_FILE:on1xc>" "$<TARGET_FILE:on1x_rt_sdk>")
        set_tests_properties(on1x.runtime.ffi_embedding_${_mode}
            PROPERTIES TIMEOUT 240)
    endforeach()
endif()
