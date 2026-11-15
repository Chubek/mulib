# Configure-time evidence only. Never fetch, update, or edit a supplied checkout.
find_package(Git QUIET)
set(ON1X_LIBSTONE_REVISION "unavailable")
if(GIT_FOUND AND EXISTS "${ON1X_LIBSTONE_ROOT}/.git")
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ON1X_LIBSTONE_ROOT}" rev-parse HEAD
    RESULT_VARIABLE _revision_status OUTPUT_VARIABLE _revision ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_revision_status EQUAL 0)
    set(ON1X_LIBSTONE_REVISION "${_revision}")
  endif()
endif()
message(STATUS "On1x LibStone: ${ON1X_LIBSTONE_ROOT} (${ON1X_LIBSTONE_REVISION})")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/on1x-build-info.txt"
  "schema=on1x.build-info/v1\nlibstone_root=${ON1X_LIBSTONE_ROOT}\nlibstone_revision=${ON1X_LIBSTONE_REVISION}\n"
  "scanner=${ON1X_SCANNER_DESCRIPTION}\nc_compiler=${CMAKE_C_COMPILER}\nc_version=${CMAKE_C_COMPILER_VERSION}\n"
  "cxx_compiler=${CMAKE_CXX_COMPILER}\ncxx_version=${CMAKE_CXX_COMPILER_VERSION}\n"
  "system=${CMAKE_SYSTEM_NAME}\nprocessor=${CMAKE_SYSTEM_PROCESSOR}\n"
  "exoml=${ON1X_WITH_EXOML}\nexoml_extract=${ON1X_WITH_EXOML_EXTRACT}\n"
  "exotract=${ON1X_WITH_EXOTRACT}\n"
  "novostone=${ON1X_WITH_NOVOSTONE}\nchisel=${ON1X_WITH_CHISEL}\nnerotypical=${ON1X_WITH_NEROTYPICAL}\n"
  "backend=c11\nmir=scalar-v1\nmir_passes=builtin-v1-opt-in\nnative=unimplemented\nsafe_ffi=unimplemented\n")
