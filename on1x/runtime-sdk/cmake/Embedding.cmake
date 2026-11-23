get_filename_component(_rt_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
install(FILES "${_rt_root}/README.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/runtime-sdk" COMPONENT On1xRT)
if(ON1X_RT_BUILD_SHARED)
    install(FILES "${_rt_root}/python/on1x_rt.py"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/on1x-runtime/python" COMPONENT On1xRT)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    set(_rt_choose "${CMAKE_CURRENT_BINARY_DIR}/choose.inc")
    add_custom_command(OUTPUT "${_rt_choose}"
        COMMAND on1xc --no-prelude --emit-c "${_rt_root}/tests/choose.on1x" -o "${_rt_choose}"
        DEPENDS on1xc "${_rt_root}/tests/choose.on1x" VERBATIM)
    add_library(on1x_rt_choose OBJECT "${_rt_root}/tests/choose_provider.c" "${_rt_choose}")
    set_target_properties(on1x_rt_choose PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(on1x_rt_choose PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${_rt_root}/../runtime")
    target_link_libraries(on1x_rt_choose PUBLIC On1x::RT)
    foreach(lang IN ITEMS c cpp)
        add_executable(on1x_rt_embed_${lang} "${_rt_root}/tests/embedding.${lang}")
        target_link_libraries(on1x_rt_embed_${lang} PRIVATE on1x_rt_choose)
        add_test(NAME on1x.runtime.embedding_${lang} COMMAND on1x_rt_embed_${lang})
    endforeach()
    if(ON1X_RT_BUILD_SHARED AND NOT CMAKE_CROSSCOMPILING)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_library(on1x_rt_python_fixture MODULE)
        target_link_libraries(on1x_rt_python_fixture PRIVATE on1x_rt_choose)
        add_test(NAME on1x.runtime.embedding_python
            COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${_rt_root}/python"
                "PYTHONDONTWRITEBYTECODE=1" "${Python3_EXECUTABLE}" "${_rt_root}/tests/embedding.py"
                "$<TARGET_FILE:on1x_rt_sdk>" "$<TARGET_FILE:on1x_rt_python_fixture>")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_test(NAME on1x.runtime.embedding_relocated
                COMMAND "${Python3_EXECUTABLE}" "${_rt_root}/tests/relocated.py"
                    "${CMAKE_COMMAND}" "${CMAKE_BINARY_DIR}" "$<CONFIG>"
                    "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}" "${_rt_choose}")
        endif()
    endif()
endif()
