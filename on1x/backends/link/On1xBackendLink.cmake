include_guard(GLOBAL)

set(ON1X_YELLOWSTONE_HOST_GENERATOR "" CACHE FILEPATH
  "Absolute path to a build-host on1x-backend-manifest executable (for cross builds)")
set(_on1x_yellowstone_default OFF)
if(ON1X_YELLOWSTONE_HOST_GENERATOR OR
    (NOT CMAKE_CROSSCOMPILING AND
     EXISTS "${ON1X_LIBSTONE_ROOT}/yellowstone/ld/BackendManifest.cpp" AND
     EXISTS "${ON1X_LIBSTONE_ROOT}/scripts/aurocks.pl"))
  set(_on1x_yellowstone_default ON)
endif()
option(ON1X_USE_YELLOWSTONE_LINK "Generate the built-in registrar map with LibStone Yellowstone"
  ${_on1x_yellowstone_default})
unset(_on1x_yellowstone_default)

# Called in the same directory that creates the target containing Builtin.cpp.
function(on1x_link_backend_artifacts target)
  if(NOT ON1X_USE_YELLOWSTONE_LINK)
    message(STATUS "On1x backend link: portable built-in registrar map")
    return()
  endif()

  set(_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  if(ON1X_YELLOWSTONE_HOST_GENERATOR)
    if(NOT IS_ABSOLUTE "${ON1X_YELLOWSTONE_HOST_GENERATOR}" OR
       NOT EXISTS "${ON1X_YELLOWSTONE_HOST_GENERATOR}" OR
       IS_DIRECTORY "${ON1X_YELLOWSTONE_HOST_GENERATOR}")
      message(FATAL_ERROR "ON1X_YELLOWSTONE_HOST_GENERATOR must name an absolute build-host executable path")
    endif()
    set(_generator "${ON1X_YELLOWSTONE_HOST_GENERATOR}")
    set(_tool_dep "${_generator}")
  elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
      "ON1X_USE_YELLOWSTONE_LINK during cross compilation requires a native "
      "ON1X_YELLOWSTONE_HOST_GENERATOR, or configure with ON1X_USE_YELLOWSTONE_LINK=OFF")
  else()
    if(NOT TARGET on1x-backend-manifest)
      add_subdirectory("${_source}" "${CMAKE_CURRENT_BINARY_DIR}/yellowstone-host" EXCLUDE_FROM_ALL)
    endif()
    set(_generator "$<TARGET_FILE:on1x-backend-manifest>")
    set(_tool_dep on1x-backend-manifest)
  endif()

  set(_spec "${_source}/backends.lua")
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/registrars/${target}/$<CONFIG>")
  set(_header "${_dir}/BuiltinRegistrars.inc")
  add_custom_command(
    OUTPUT "${_header}"
    BYPRODUCTS "${_dir}/backends.lua"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dir}"
    # Keep generator arguments relative so its argv is ASCII even for a
    # non-ASCII checkout/build path on Windows.
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_spec}" "${_dir}/backends.lua"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${_dir}" "${_generator}" backends.lua BuiltinRegistrars.inc
    DEPENDS "${_spec}" ${_tool_dep}
    COMMENT "Generating the On1x built-in registrar map with LibStone Yellowstone"
    VERBATIM)
  target_sources(${target} PRIVATE "${_header}")
  target_include_directories(${target} BEFORE PRIVATE "${_dir}")
  message(STATUS "On1x backend link: Yellowstone Lua registrar map + target C++ toolchain")

  if(BUILD_TESTING)
    add_test(NAME on1x.backends.yellowstone
      COMMAND "${Python3_EXECUTABLE}" "${_source}/tests/check_registrars.py"
        "${_generator}" "${_spec}" "${_header}")
    set_tests_properties(on1x.backends.yellowstone PROPERTIES TIMEOUT 30)
  endif()
endfunction()
