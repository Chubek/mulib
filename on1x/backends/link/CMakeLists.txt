cmake_minimum_required(VERSION 3.20)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(On1xBackendManifestHost LANGUAGES C CXX)
  set(ON1X_LIBSTONE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libstone"
    CACHE PATH "LibStone source checkout")
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "on1x-backend-manifest must be built with the build-host toolchain")
  endif()
endif()

# Reuse a superbuild's target, an explicitly selected installed SDK, or the
# configured checkout. The nested build is excluded from On1x installation.
if(NOT TARGET qobjld::qobjld)
  set(_yellowstone "${ON1X_LIBSTONE_ROOT}/yellowstone")
  if(NOT Yellowstone_DIR AND EXISTS "${_yellowstone}/ld/BackendManifest.cpp")
    set(YELLOWSTONE_BUILD_TOOLS OFF)
    set(YELLOWSTONE_BUILD_TESTS OFF)
    set(QOBJFILE_DOMTERM OFF)
    add_subdirectory("${_yellowstone}" "${CMAKE_CURRENT_BINARY_DIR}/yellowstone" EXCLUDE_FROM_ALL)
  else()
    find_package(Yellowstone CONFIG QUIET)
    if(NOT TARGET qobjld::qobjld)
      message(FATAL_ERROR
        "Yellowstone with qld_backend_manifest is required: set ON1X_LIBSTONE_ROOT "
        "to a compatible checkout or include its installed prefix in CMAKE_PREFIX_PATH. "
        "Otherwise configure with ON1X_USE_YELLOWSTONE_LINK=OFF")
    endif()
    include(CheckCXXSymbolExists)
    set(CMAKE_REQUIRED_LIBRARIES qobjld::qobjld)
    unset(_on1x_has_backend_manifest CACHE)
    check_cxx_symbol_exists(qld_backend_manifest "ld.h" _on1x_has_backend_manifest)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(NOT _on1x_has_backend_manifest)
      message(FATAL_ERROR "The selected Yellowstone package lacks qld_backend_manifest")
    endif()
  endif()
endif()

add_executable(on1x-backend-manifest GenerateRegistrars.cpp)
target_compile_features(on1x-backend-manifest PRIVATE cxx_std_20)
target_link_libraries(on1x-backend-manifest PRIVATE qobjld::qobjld)
