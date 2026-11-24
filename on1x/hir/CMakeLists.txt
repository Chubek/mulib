target_sources(on1x_frontend PRIVATE
  optimize.cpp lower_mir.cpp vm_closures.cpp analysis_directives.cpp
  On1xMETA/processor.cpp On1xMETA/processor_semantic.cpp On1xMETA/partial_eval.cpp)
add_subdirectory(On1xMETA)
add_subdirectory(MEMGraph)

# Only the driver selects the optional LibStone HIR passes. The canonical
# frontend archive (including SDK consumers) needs neither provider to link.
add_library(on1x_hir_adapters INTERFACE)
if(ON1X_WITH_FIRESTONE_HIR)
  if(NOT EXISTS "${ON1X_LIBSTONE_ROOT}/firestone/libsema/OverflowCheck.hpp" OR
     NOT EXISTS "${ON1X_LIBSTONE_ROOT}/firestone/librewrite/Term.hpp")
    message(FATAL_ERROR "ON1X_WITH_FIRESTONE_HIR needs Firestone under ON1X_LIBSTONE_ROOT")
  endif()
  target_sources(on1x_hir_adapters INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/firestone.cpp")
  target_include_directories(on1x_hir_adapters SYSTEM INTERFACE "${ON1X_LIBSTONE_ROOT}/firestone")
  target_compile_definitions(on1x_hir_adapters INTERFACE ON1X_WITH_FIRESTONE_HIR=1)
endif()
