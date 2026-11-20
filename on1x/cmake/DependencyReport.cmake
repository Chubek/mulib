# Configure-time evidence only. Never fetch, update, or edit a supplied checkout.
find_package(Git QUIET)
set(ON1X_SOURCE_REVISION "unavailable")
set(ON1X_SOURCE_TRACKED_DIRTY "unknown")
set(ON1X_LIBSTONE_REVISION "unavailable")
if(GIT_FOUND AND (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" OR
                  EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.git"))
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse --verify HEAD
    RESULT_VARIABLE _revision_status OUTPUT_VARIABLE _revision ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_revision_status EQUAL 0)
    set(ON1X_SOURCE_REVISION "${_revision}")
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" status
      --porcelain --untracked-files=no --ignore-submodules=all
      RESULT_VARIABLE _status_result OUTPUT_VARIABLE _status_output ERROR_QUIET)
    if(_status_result EQUAL 0)
      if(_status_output STREQUAL "")
        set(ON1X_SOURCE_TRACKED_DIRTY "no")
      else()
        set(ON1X_SOURCE_TRACKED_DIRTY "yes")
      endif()
    endif()
  endif()
endif()
if(GIT_FOUND AND EXISTS "${ON1X_LIBSTONE_ROOT}/.git")
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ON1X_LIBSTONE_ROOT}" rev-parse --verify HEAD
    RESULT_VARIABLE _revision_status OUTPUT_VARIABLE _revision ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_revision_status EQUAL 0)
    set(ON1X_LIBSTONE_REVISION "${_revision}")
  endif()
endif()
message(STATUS "On1x LibStone: ${ON1X_LIBSTONE_ROOT} (${ON1X_LIBSTONE_REVISION})")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/on1x-build-info.txt"
  "schema=on1x.build-info/v1\nsource_revision=${ON1X_SOURCE_REVISION}\n"
  "source_tracked_dirty=${ON1X_SOURCE_TRACKED_DIRTY}\n"
  "libstone_root=${ON1X_LIBSTONE_ROOT}\nlibstone_revision=${ON1X_LIBSTONE_REVISION}\n"
  "scanner=${ON1X_SCANNER_DESCRIPTION}\nc_compiler=${CMAKE_C_COMPILER}\nc_version=${CMAKE_C_COMPILER_VERSION}\n"
  "cxx_compiler=${CMAKE_CXX_COMPILER}\ncxx_version=${CMAKE_CXX_COMPILER_VERSION}\n"
  "system=${CMAKE_SYSTEM_NAME}\nprocessor=${CMAKE_SYSTEM_PROCESSOR}\n"
  "exoml=${ON1X_WITH_EXOML}\nexoml_extract=${ON1X_WITH_EXOML_EXTRACT}\n"
  "exotract=${ON1X_WITH_EXOTRACT}\nexotract_lua=${ON1X_WITH_EXOTRACT_LUA}\n"
   "novostone=${ON1X_WITH_NOVOSTONE}\nchisel=${ON1X_WITH_CHISEL}\nnerotypical=${ON1X_WITH_NEROTYPICAL}\nsyren=${ON1X_WITH_SYREN}\n"
  "backend=c11\nmir=bounded-v1-v2-v3\nmir_passes=builtin-v1-opt-in\nnative=unimplemented\n")
if(ON1X_WITH_EXOTRACT_LUA)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/on1x-build-info.txt" "safe_ffi=bounded-kernelbind-signed-review-required\n")
elseif(ON1X_WITH_EXOTRACT)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/on1x-build-info.txt" "safe_ffi=bundled-kernelbind-lua-disabled\n")
else()
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/on1x-build-info.txt" "safe_ffi=bundled-kernelbind-extraction-disabled\n")
endif()
