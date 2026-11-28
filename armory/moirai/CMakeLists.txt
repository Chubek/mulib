cmake_minimum_required(VERSION 3.16)
project(domscript LANGUAGES NONE)
include(GNUInstallDirs)

find_program(GO_EXECUTABLE go REQUIRED)
find_program(GIT_EXECUTABLE git)

option(DOMSCRIPT_BUILD_STDLIB "Build Domscript's standard library" ON)
option(DOMSCRIPT_ENABLE_KEYSTONE "Enable Domscript's Keystone native JIT" ON)
set(DOMSCRIPT_REPL_BACKEND "domrl" CACHE STRING "REPL backend: domrl, replxx, or isocline")
set_property(CACHE DOMSCRIPT_REPL_BACKEND PROPERTY STRINGS domrl replxx isocline)
if(NOT DOMSCRIPT_REPL_BACKEND STREQUAL "domrl" AND NOT DOMSCRIPT_REPL_BACKEND STREQUAL "replxx" AND NOT DOMSCRIPT_REPL_BACKEND STREQUAL "isocline")
  message(FATAL_ERROR "DOMSCRIPT_REPL_BACKEND must be domrl, replxx, or isocline")
endif()

# Output into the build tree, not the source tree. When this file is pulled in
# with add_subdirectory(), CMAKE_CURRENT_BINARY_DIR is <build>/<subdir>, so
# `cmake --build build` drops artifacts under build/. A parent project may set
# DOMSCRIPT_DIST_DIR before add_subdirectory() to override.
if(NOT DOMSCRIPT_DIST_DIR)
  set(DOMSCRIPT_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
endif()
# Imported consumers validate the generated header's include directory at
# configure time, before the C-shared runtime has been built.
file(MAKE_DIRECTORY "${DOMSCRIPT_DIST_DIR}")

# Overrides mirroring `./build.sh <GOOS> <GOARCH>` and the DS_VERSION env var.
set(DOMSCRIPT_GOOS   "" CACHE STRING "Target GOOS (empty = host via 'go env')")
set(DOMSCRIPT_GOARCH "" CACHE STRING "Target GOARCH (empty = host via 'go env')")
set(DS_VERSION       "0.0.0-dev" CACHE STRING "Version embedded into the build")

if (DOMSCRIPT_BUILD_STDLIB)
  add_subdirectory(stdlib)
endif()

# --- resolve GOOS/GOARCH (script used `go env` as the default) --------
if(DOMSCRIPT_GOOS STREQUAL "")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env "GOROOT=/usr/lib/go" "${GO_EXECUTABLE}" env GOOS
    OUTPUT_VARIABLE DOMSCRIPT_GOOS_RESOLVED OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
else()
  set(DOMSCRIPT_GOOS_RESOLVED "${DOMSCRIPT_GOOS}")
endif()
if(DOMSCRIPT_GOARCH STREQUAL "")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env "GOROOT=/usr/lib/go" "${GO_EXECUTABLE}" env GOARCH
    OUTPUT_VARIABLE DOMSCRIPT_GOARCH_RESOLVED OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
else()
  set(DOMSCRIPT_GOARCH_RESOLVED "${DOMSCRIPT_GOARCH}")
endif()
message(STATUS "domscript: building for ${DOMSCRIPT_GOOS_RESOLVED}/${DOMSCRIPT_GOARCH_RESOLVED}")

# --- short git SHA for the ldflags ------------------------------------
set(DOMSCRIPT_GIT_SHA "unknown")
if(GIT_EXECUTABLE)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE DOMSCRIPT_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(DOMSCRIPT_GIT_SHA STREQUAL "")
    set(DOMSCRIPT_GIT_SHA "unknown")
  endif()
endif()

set(DOMSCRIPT_LDFLAGS
  "-X domscript/capi.buildVersion=${DS_VERSION} -X domscript/capi.buildSHA=${DOMSCRIPT_GIT_SHA}")

# --- per-OS output names (same table as the script) --------------------
if(DOMSCRIPT_GOOS_RESOLVED STREQUAL "windows")
  set(DOMSCRIPT_LIB_NAME "domscript.dll")
  set(DOMSCRIPT_BIN_NAME "dsc.exe")
elseif(DOMSCRIPT_GOOS_RESOLVED STREQUAL "darwin")
  set(DOMSCRIPT_LIB_NAME "libdomscript.dylib")
  set(DOMSCRIPT_BIN_NAME "dsc")
else()
  set(DOMSCRIPT_LIB_NAME "libdomscript.so")
  set(DOMSCRIPT_BIN_NAME "dsc")
endif()
set(DOMSCRIPT_LIB "${DOMSCRIPT_DIST_DIR}/${DOMSCRIPT_LIB_NAME}")
set(DOMSCRIPT_BIN "${DOMSCRIPT_DIST_DIR}/${DOMSCRIPT_BIN_NAME}")
set(DOMSCRIPT_HDR "${DOMSCRIPT_DIST_DIR}/domscript.h")

# go's c-shared mode names the header after the output library
# (libdomscript.so -> libdomscript.h); the script normalized it to domscript.h.
get_filename_component(_lib_stem "${DOMSCRIPT_LIB_NAME}" NAME_WE)
set(DOMSCRIPT_RAW_HDR "${DOMSCRIPT_DIST_DIR}/${_lib_stem}.h")

# Environment shared by both go builds (script required CGO_ENABLED=1).
set(DOMSCRIPT_GO_ENV
  ${CMAKE_COMMAND} -E env
    "GOROOT=/usr/lib/go"
    "GOCACHE=${CMAKE_BINARY_DIR}/go-cache"
    CGO_ENABLED=1
    "GOOS=${DOMSCRIPT_GOOS_RESOLVED}"
    "GOARCH=${DOMSCRIPT_GOARCH_RESOLVED}")

set(DOMSCRIPT_GO_TAG_ARGS)
if(DOMSCRIPT_ENABLE_KEYSTONE AND DOMSCRIPT_GOOS_RESOLVED STREQUAL "linux" AND DOMSCRIPT_GOARCH_RESOLVED STREQUAL "amd64")
  set(DOMSCRIPT_KEYSTONE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/keystone-build.sh")
  add_custom_target(domscript_keystone
    COMMAND bash "${DOMSCRIPT_KEYSTONE_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building Keystone for domscript"
    VERBATIM)
  set(DOMSCRIPT_GO_TAG_ARGS -tags keystone)
endif()

set(DOMSCRIPT_REPL_TAG_ARGS)
set(DOMSCRIPT_DOMRL_DEPENDS)
if(DOMSCRIPT_REPL_BACKEND STREQUAL "domrl")
  if(NOT TARGET domrl OR NOT TARGET termscript_stdlib OR NOT TARGET termscript_standalone)
    message(FATAL_ERROR "DOMSCRIPT_REPL_BACKEND=domrl requires DomRL and DomTERM targets")
  endif()
  set(DOMSCRIPT_REPL_TAG_ARGS -tags domrl)
  list(APPEND DOMSCRIPT_GO_ENV
    "CGO_LDFLAGS=$<TARGET_FILE:domrl> $<TARGET_FILE:termscript_stdlib> $<TARGET_FILE:termscript_standalone> -pthread")
  set(DOMSCRIPT_DOMRL_DEPENDS domrl termscript_stdlib termscript_standalone)
elseif(DOMSCRIPT_REPL_BACKEND STREQUAL "replxx")
  set(DOMSCRIPT_REPL_TAG_ARGS -tags replxx)
elseif(DOMSCRIPT_REPL_BACKEND STREQUAL "isocline")
  set(DOMSCRIPT_REPL_TAG_ARGS -tags isocline)
endif()

file(GLOB_RECURSE DOMSCRIPT_GO_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.go"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/go.mod"
  "${CMAKE_CURRENT_SOURCE_DIR}/go.sum"
)

# Build-time script that normalizes the header and warns if the main C entrypoint is missing.
set(DOMSCRIPT_HDR_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/domscript_fix_header.cmake")
file(WRITE "${DOMSCRIPT_HDR_SCRIPT}" [=[
if(NOT RAW_HDR STREQUAL HDR AND EXISTS "${RAW_HDR}")
  file(RENAME "${RAW_HDR}" "${HDR}")
endif()
if(NOT EXISTS "${HDR}")
  message(FATAL_ERROR "domscript: expected generated header ${HDR} not found")
endif()
file(READ "${HDR}" _hdr_contents)
string(FIND "${_hdr_contents}" "DomscriptOpen" _pos)
if(_pos EQUAL -1)
  message(WARNING "domscript: 'DomscriptOpen' not found in ${HDR}; the C API may be incomplete")
endif()
]=])

# --- c-shared runtime library ------------------------------------------
add_custom_command(
  OUTPUT  "${DOMSCRIPT_LIB}" "${DOMSCRIPT_HDR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DOMSCRIPT_DIST_DIR}"
  COMMAND ${DOMSCRIPT_GO_ENV} "${GO_EXECUTABLE}" build
          -buildmode=c-shared
          ${DOMSCRIPT_GO_TAG_ARGS}
          -ldflags "${DOMSCRIPT_LDFLAGS}"
          -o "${DOMSCRIPT_LIB}"
          ./capi
  COMMAND ${CMAKE_COMMAND}
          -D "RAW_HDR=${DOMSCRIPT_RAW_HDR}"
          -D "HDR=${DOMSCRIPT_HDR}"
          -P "${DOMSCRIPT_HDR_SCRIPT}"
  DEPENDS ${DOMSCRIPT_GO_SOURCES} ${DOMSCRIPT_DOMRL_DEPENDS}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "go build -buildmode=c-shared -> ${DOMSCRIPT_LIB_NAME}"
  VERBATIM
)

# --- dsc executable -----------------------------------------------------
add_custom_command(
  OUTPUT  "${DOMSCRIPT_BIN}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DOMSCRIPT_DIST_DIR}"
  COMMAND ${DOMSCRIPT_GO_ENV} "${GO_EXECUTABLE}" build
          ${DOMSCRIPT_REPL_TAG_ARGS}
          ${DOMSCRIPT_GO_TAG_ARGS}
          -ldflags "${DOMSCRIPT_LDFLAGS}"
          -o "${DOMSCRIPT_BIN}"
          ./cmd/dsc
  DEPENDS ${DOMSCRIPT_GO_SOURCES}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "go build -> ${DOMSCRIPT_BIN_NAME}"
  VERBATIM
)

add_custom_target(domscript_runtime_build
  DEPENDS "${DOMSCRIPT_LIB}" "${DOMSCRIPT_HDR}"
)
add_custom_target(domscript_build ALL
  DEPENDS "${DOMSCRIPT_BIN}"
)
add_dependencies(domscript_build domscript_runtime_build)
if(TARGET domscript_keystone)
  add_dependencies(domscript_runtime_build domscript_keystone)
endif()

# --- consumable target for C/C++ embedders ------------------------------
add_library(domscript SHARED IMPORTED GLOBAL)
add_dependencies(domscript domscript_runtime_build)
set_target_properties(domscript PROPERTIES
  IMPORTED_LOCATION "${DOMSCRIPT_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${DOMSCRIPT_DIST_DIR}"
)
if(DOMSCRIPT_GOOS_RESOLVED STREQUAL "windows")
  set_target_properties(domscript PROPERTIES IMPORTED_IMPLIB "${DOMSCRIPT_LIB}")
endif()
add_library(domscript::domscript ALIAS domscript)

add_custom_target(domscript_clean
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${DOMSCRIPT_DIST_DIR}"
  COMMENT "Removing ${DOMSCRIPT_DIST_DIR}"
)

install(PROGRAMS "${DOMSCRIPT_BIN}"
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${DOMSCRIPT_LIB}"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES "${DOMSCRIPT_HDR}"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
