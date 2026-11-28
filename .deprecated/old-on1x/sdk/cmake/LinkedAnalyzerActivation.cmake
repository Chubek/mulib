# Activate an explicitly selected trusted linked analyzer after on1xc and Python3.
set(ON1X_LINKED_SDK_EXTENSION_V2 "" CACHE FILEPATH
  "Trusted C/C++ provider defining on1x_sdk_linked_extension_v2 for explicit analyzer hooks")

if(ON1X_LINKED_SDK_EXTENSION_V2)
  if(NOT ON1X_BUILD_EXTENSION_SDK OR NOT TARGET on1xc)
    message(FATAL_ERROR "ON1X_LINKED_SDK_EXTENSION_V2 requires an SDK-enabled on1xc")
  endif()
  target_sources(on1xc PRIVATE "${ON1X_LINKED_SDK_EXTENSION_V2}")
  target_compile_definitions(on1xc PRIVATE ON1X_WITH_LINKED_SDK_EXTENSION_V2=1)
endif()

get_filename_component(_sdk_v2_fixture
  "${CMAKE_CURRENT_LIST_DIR}/../tests/linked_extension_v2.c" ABSOLUTE)
if(ON1X_BUILD_EXTENSION_SDK AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  if("${ON1X_LINKED_SDK_EXTENSION_V2}" STREQUAL "${_sdk_v2_fixture}")
    add_test(NAME on1x.sdk.linked_analyzer_v2
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/../tests/check_linked_v2.py"
        "$<TARGET_FILE:on1xc>")
    set_tests_properties(on1x.sdk.linked_analyzer_v2 PROPERTIES TIMEOUT 60)
  elseif(NOT ON1X_LINKED_SDK_EXTENSION_V2)
    add_test(NAME on1x.sdk.linked_analyzer_v2_unavailable
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/../tests/check_linked_v2.py"
        "$<TARGET_FILE:on1xc>" --unavailable)
    set_tests_properties(on1x.sdk.linked_analyzer_v2_unavailable PROPERTIES TIMEOUT 30)
  endif()
endif()
unset(_sdk_v2_fixture)
