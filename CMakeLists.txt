cmake_minimum_required(VERSION 3.16)
project(MuLIB VERSION 0.1.0 LANGUAGES C)

find_package(Threads REQUIRED)
add_library(mulib INTERFACE)
target_include_directories(mulib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mulib INTERFACE c_std_11)
target_link_libraries(mulib INTERFACE Threads::Threads)

option(MULIB_BUILD_MUW3 "Build MuW3 Weblet runtime and MicroChan daemon" ON)

option(MULIB_BUILD_SHIRAZ "Build the Linux Shiraz runtime libraries and tools" ${CMAKE_HOST_UNIX})
if(MULIB_BUILD_SHIRAZ AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory(library/shiraz)
endif()

# Opt-in header-only provider layer; implementation placement remains caller-owned.
add_library(mulib_yantlr_stdlib INTERFACE)
target_include_directories(mulib_yantlr_stdlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/library/yantlr)
target_link_libraries(mulib_yantlr_stdlib INTERFACE mulib)
if(NOT WIN32)
  target_link_libraries(mulib_yantlr_stdlib INTERFACE m)
endif()

option(MULIB_BUILD_VIRTIO "Build separately compiled VMM Virtio frontend/backend libraries" ON)
if(MULIB_BUILD_VIRTIO)
  add_library(mulib_virtio_protocol STATIC library/virtio-os/protocol.c)
  target_link_libraries(mulib_virtio_protocol PUBLIC mulib)
  file(GLOB virtio_os_devices CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/library/virtio-os/*.c")
  list(REMOVE_ITEM virtio_os_devices "${CMAKE_CURRENT_SOURCE_DIR}/library/virtio-os/protocol.c")
  add_library(mulib_virtio_os STATIC ${virtio_os_devices})
  target_include_directories(mulib_virtio_os PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/virtio-os)
  target_link_libraries(mulib_virtio_os PUBLIC mulib_virtio_protocol)
  file(GLOB virtio_guest_devices CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/library/virtio-guest/*.c")
  add_library(mulib_virtio_guest STATIC ${virtio_guest_devices})
  target_include_directories(mulib_virtio_guest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/virtio-guest)
  target_link_libraries(mulib_virtio_guest PUBLIC mulib_virtio_protocol)
endif()

option(MULIB_BUILD_TESTS "Build MuLIB tests" ON)
option(MULIB_BUILD_VULKAN_TESTS "Build the live headless Vulkan device test (Linux)" OFF)
option(MULIB_BUILD_MINIMPEG_VIDEO_TESTS "Build tests for the optional MiniMPEG video backend" OFF)
option(MULIB_BUILD_SECRET_OPENSSL_TESTS "Test the optional secret EVP backend with OpenSSL" OFF)
option(MULIB_BUILD_SSH_OPENSSL "Build the OpenSSL-backed SSH example and focused tests" OFF)
option(MULIB_BUILD_SSH_OPENSSH_TESTS "Run the local OpenSSH client/server interoperability fixture" OFF)
option(MULIB_BUILD_MODULE_EXAMPLES "Build standalone example for each module" OFF)
option(MULIB_STRICT_WARNINGS "Treat warnings in MuLIB tests/examples as errors" ON)
option(MULIB_SANITIZERS "Run MuLIB tests/examples with address and UB sanitizers" OFF)
option(MULIB_BUILD_FUZZER "Build the Clang libFuzzer hostile-input target" OFF)

# Flags belong to our executables, not to downstream users of the INTERFACE target.
function(mulib_test_executable target)
  target_link_libraries(${target} PRIVATE mulib ${CMAKE_DL_LIBS})
  if(NOT WIN32)
    target_link_libraries(${target} PRIVATE m)
  endif()
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Examples intentionally use assertions as executable checks, including in Release.
    target_compile_options(${target} PRIVATE -UNDEBUG)
    if(MULIB_STRICT_WARNINGS)
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(MULIB_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /UNDEBUG)
  endif()
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/${target}")
endfunction()

function(mulib_register_test target)
  if(target STREQUAL "mulib_test-elvi-scrape" OR target STREQUAL "mulib_test-elvi-scrape-providers")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}")
  elseif(target STREQUAL "mulib_test_git_exec")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library/git")
  elseif(target STREQUAL "mulib_test_archml" OR target STREQUAL "mulib_test_isel")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library/archml")
  elseif(target STREQUAL "mulib_test_immgtk" OR target STREQUAL "mulib_example_immgtk")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library")
  elseif(target STREQUAL "mulib_publish_example")
    add_test(NAME ${target} COMMAND ${target} ".")
  elseif(target STREQUAL "mulib_test_imtui_mune")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}")
  else()
    add_test(NAME ${target} COMMAND ${target})
  endif()
  set_tests_properties(${target} PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/${target}" TIMEOUT 60)
endfunction()

if(MULIB_BUILD_SSH_OPENSSL)
  find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)
  foreach(example kernels connection)
    add_executable(mulib_example_ssh_${example} examples/ssh/${example}.c)
    target_link_libraries(mulib_example_ssh_${example} PRIVATE OpenSSL::Crypto)
    mulib_test_executable(mulib_example_ssh_${example})
  endforeach()
endif()
if(MULIB_BUILD_SSH_OPENSSH_TESTS AND (NOT MULIB_BUILD_SSH_OPENSSL OR NOT MULIB_BUILD_TESTS
    OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING))
  message(FATAL_ERROR "The OpenSSH fixture requires native Linux with MULIB_BUILD_TESTS and MULIB_BUILD_SSH_OPENSSL")
endif()

if(MULIB_BUILD_VIRTIO)
  foreach(target mulib_virtio_protocol mulib_virtio_os mulib_virtio_guest)
    mulib_test_executable(${target})
  endforeach()
endif()

if(MULIB_BUILD_MUW3)
  add_subdirectory(library/lmuw3)
endif()

if(MULIB_BUILD_TESTS)
  enable_testing()
  if(MULIB_BUILD_SSH_OPENSSL)
    mulib_register_test(mulib_example_ssh_kernels)
    mulib_register_test(mulib_example_ssh_connection)
    add_library(mulib_ssh_test_implementation OBJECT tests/ssh/wire.c tests/ssh/crypto.c tests/ssh/session.c)
    target_link_libraries(mulib_ssh_test_implementation PRIVATE OpenSSL::Crypto)
    mulib_test_executable(mulib_ssh_test_implementation)
    if(MULIB_BUILD_SSH_OPENSSH_TESTS)
      find_package(Python3 REQUIRED COMPONENTS Interpreter)
      add_executable(mulib_ssh_interop tests/ssh/interop.c $<TARGET_OBJECTS:mulib_ssh_test_implementation>)
      target_link_libraries(mulib_ssh_interop PRIVATE OpenSSL::Crypto)
      mulib_test_executable(mulib_ssh_interop)
      add_test(NAME mulib_ssh_interop COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/ssh/openssh.py" $<TARGET_FILE:mulib_ssh_interop>)
      set_tests_properties(mulib_ssh_interop PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    endif()
  endif()
  find_program(MULIB_GLSLANG_VALIDATOR NAMES glslangValidator glslang)
  if(MULIB_BUILD_VULKAN_TESTS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
      message(FATAL_ERROR "MULIB_BUILD_VULKAN_TESTS requires a native Linux build")
    endif()
    find_path(MULIB_VULKAN_INCLUDE_DIR NAMES vulkan/vulkan_core.h
      HINTS ENV VULKAN_SDK PATH_SUFFIXES include)
    if(NOT MULIB_VULKAN_INCLUDE_DIR)
      message(FATAL_ERROR "MULIB_BUILD_VULKAN_TESTS requires Vulkan development headers")
    endif()
    add_executable(mulib_test_vulkan_device tests/graphics/vulkan-device.c)
    target_compile_definitions(mulib_test_vulkan_device PRIVATE MULIB_GLX_VULKAN_ENABLE)
    target_include_directories(mulib_test_vulkan_device SYSTEM PRIVATE "${MULIB_VULKAN_INCLUDE_DIR}")
    mulib_test_executable(mulib_test_vulkan_device)
    mulib_register_test(mulib_test_vulkan_device)
    set_tests_properties(mulib_test_vulkan_device PROPERTIES SKIP_RETURN_CODE 77)
    if(MULIB_GLSLANG_VALIDATOR)
      set(vulkan_compute_shader "${CMAKE_CURRENT_BINARY_DIR}/vulkan-compute.spv")
      add_custom_command(OUTPUT "${vulkan_compute_shader}"
        COMMAND "${MULIB_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
          -o "${vulkan_compute_shader}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/graphics/vulkan-compute.comp"
        DEPENDS tests/graphics/vulkan-compute.comp VERBATIM)
      add_custom_target(mulib_vulkan_compute_shader DEPENDS "${vulkan_compute_shader}")
      add_executable(mulib_test_vulkan_compute tests/graphics/vulkan-compute.c)
      add_dependencies(mulib_test_vulkan_compute mulib_vulkan_compute_shader)
      target_compile_definitions(mulib_test_vulkan_compute PRIVATE MULIB_GLX_VULKAN_ENABLE)
      target_include_directories(mulib_test_vulkan_compute SYSTEM PRIVATE "${MULIB_VULKAN_INCLUDE_DIR}")
      mulib_test_executable(mulib_test_vulkan_compute)
      add_test(NAME mulib_test_vulkan_compute COMMAND mulib_test_vulkan_compute "${vulkan_compute_shader}")
      set_tests_properties(mulib_test_vulkan_compute PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_test_vulkan_compute"
        SKIP_RETURN_CODE 77 TIMEOUT 60)
    endif()
  endif()
  add_executable(mulib_test_minimpeg_disabled tests/minimpeg-video.c)
  mulib_test_executable(mulib_test_minimpeg_disabled)
  mulib_register_test(mulib_test_minimpeg_disabled)
  if(MULIB_BUILD_MINIMPEG_VIDEO_TESTS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MULIB_MINIMPEG_FFMPEG REQUIRED IMPORTED_TARGET "libavcodec>=59" libavutil)
    add_executable(mulib_test_minimpeg_video tests/minimpeg-video.c)
    target_compile_definitions(mulib_test_minimpeg_video PRIVATE MULIB_MINIMPEG_USE_LIBAVCODEC)
    target_link_libraries(mulib_test_minimpeg_video PRIVATE PkgConfig::MULIB_MINIMPEG_FFMPEG)
    mulib_test_executable(mulib_test_minimpeg_video)
    mulib_register_test(mulib_test_minimpeg_video)
  endif()
  if(MULIB_BUILD_VIRTIO)
    add_executable(mulib_virtio_link examples/modules/virtio-guest.c)
    target_compile_definitions(mulib_virtio_link PRIVATE MULIB_VIRTIO_EXAMPLE_COMPILED)
    target_link_libraries(mulib_virtio_link PRIVATE mulib_virtio_os mulib_virtio_guest)
    mulib_test_executable(mulib_virtio_link)
    mulib_register_test(mulib_virtio_link)
    add_executable(mulib_virtio_devices_link tests/test_virtio.c)
    target_compile_definitions(mulib_virtio_devices_link PRIVATE MULIB_VIRTIO_TEST_COMPILED)
    target_link_libraries(mulib_virtio_devices_link PRIVATE mulib_virtio_os mulib_virtio_guest)
    mulib_test_executable(mulib_virtio_devices_link)
    mulib_register_test(mulib_virtio_devices_link)
  endif()
  find_package(Python3 QUIET COMPONENTS Interpreter)
  if(MULIB_BUILD_MINIMPEG_VIDEO_TESTS AND Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING)
    find_program(MULIB_MPEG2ENC_EXECUTABLE NAMES mpeg2enc)
    find_program(MULIB_MPEG2DEC_EXECUTABLE NAMES mpeg2dec)
    if(MULIB_MPEG2ENC_EXECUTABLE AND MULIB_MPEG2DEC_EXECUTABLE)
      add_test(NAME mulib_minimpeg_video_interoperability
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-minimpeg-video.py"
          --codec $<TARGET_FILE:mulib_test_minimpeg_video>
          --mpeg2enc "${MULIB_MPEG2ENC_EXECUTABLE}" --mpeg2dec "${MULIB_MPEG2DEC_EXECUTABLE}")
      set_tests_properties(mulib_minimpeg_video_interoperability PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_test_minimpeg_video" TIMEOUT 90)
    endif()
  endif()
  if(TARGET muw3 AND NOT CMAKE_CROSSCOMPILING)
    foreach(suite mock core protocols services)
      add_test(NAME mulib_muw3lib_${suite}
        COMMAND $<TARGET_FILE:muw3> --weblet
          "${CMAKE_CURRENT_SOURCE_DIR}/library/muw3lib/tests/${suite}.lua")
      set_tests_properties(mulib_muw3lib_${suite} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 20)
    endforeach()
  endif()
  add_executable(mulib_publish_example examples/build-publication.c)
  mulib_test_executable(mulib_publish_example)
  mulib_register_test(mulib_publish_example)
  if(Python3_Interpreter_FOUND)
    set_tests_properties(mulib_publish_example PROPERTIES FIXTURES_SETUP publication)
    add_test(NAME mulib_publication_formats
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify-formats.py
        "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_publish_example")
    set_tests_properties(mulib_publication_formats PROPERTIES FIXTURES_REQUIRED publication TIMEOUT 60)
    add_executable(mulib_docir_publication examples/docir-publication.c)
    mulib_test_executable(mulib_docir_publication)
    add_test(NAME mulib_docir_publication COMMAND mulib_docir_publication ".")
    set_tests_properties(mulib_docir_publication PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_docir_publication" FIXTURES_SETUP docir_publication TIMEOUT 60)
    add_test(NAME mulib_docir_formats COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify-docir-publication.py
      "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_docir_publication")
    set_tests_properties(mulib_docir_formats PROPERTIES FIXTURES_REQUIRED docir_publication TIMEOUT 180)
  endif()
  if(Python3_Interpreter_FOUND AND TARGET shiraz)
    add_test(NAME mulib_shiraz_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_shiraz.py
        --shiraz $<TARGET_FILE:shiraz> --registry $<TARGET_FILE:shiraz-reg>)
    set_tests_properties(mulib_shiraz_interoperability PROPERTIES TIMEOUT 180
      SKIP_REGULAR_EXPRESSION "SKIP: Shiraz")
  endif()
  if(Python3_Interpreter_FOUND)
    add_test(NAME mulib_virtio_amalgamation
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate_virtio.py --check)
    add_test(NAME mulib_dsymspec_snapshot
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_snapshot.py)
    set_tests_properties(mulib_dsymspec_snapshot PROPERTIES TIMEOUT 60)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
      set(_workload_flags)
      if(MULIB_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND _workload_flags --cflag=-fsanitize=address,undefined
          --cflag=-fno-sanitize-recover=all --cflag=-fno-omit-frame-pointer)
      endif()
      add_test(NAME mulib_integration_c_workloads
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/check_c.py"
          --cc "${CMAKE_C_COMPILER}" --artifacts "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/workloads"
          ${_workload_flags})
      set_tests_properties(mulib_integration_c_workloads PROPERTIES TIMEOUT 180)
    endif()
    add_test(NAME mulib_tool_regressions
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tools.py)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      add_test(NAME mulib_money_integer_reference
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_money_reference.py
          --cc ${CMAKE_C_COMPILER})
      set_tests_properties(mulib_money_integer_reference PROPERTIES TIMEOUT 60)
    endif()
    set(compiler_check_options)
    if(MULIB_SANITIZERS)
      list(APPEND compiler_check_options --sanitizers)
    endif()
    add_test(NAME mulib_compiler_integration
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_compiler.py
        --cc ${CMAKE_C_COMPILER} ${compiler_check_options})
    set_tests_properties(mulib_compiler_integration PROPERTIES TIMEOUT 300)
    add_test(NAME mulib_dependency_strata
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/stratify.py --check)
    add_test(NAME mulib_mull_inventory
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mudsl/scripts/check_mulib_inventory.py
        --include ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME mulib_mull_inventory_regressions
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mudsl/tests/test_mulib_inventory.py)
    add_test(NAME mulib_manual_inventory
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/manuals.py --check)
    add_test(NAME mulib_lprowse_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_lprowse.py --check)
    add_test(NAME mulib_lmuw3_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_lmuw3.py --check)
    add_test(NAME mulib_openai_catalog
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_openai.py --check)
    add_test(NAME mulib_opencode_catalog
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_opencode_client.py --check)
    if(UNIX)
      add_test(NAME mulib_openai_integration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openai_integration.py --cc ${CMAKE_C_COMPILER})
      set_tests_properties(mulib_openai_integration PROPERTIES TIMEOUT 120)
    endif()
    if(MULIB_BUILD_MUW3 AND UNIX)
      add_test(NAME mulib_muw3_integration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_muw3_integration.py
          --muw3 $<TARGET_FILE:muw3> --microchand $<TARGET_FILE:microchand>)
      set_tests_properties(mulib_muw3_integration PROPERTIES TIMEOUT 60)
      if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME mulib_muw3lib_loopback
          COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/library/muw3lib/tests/loopback.py"
            --muw3 $<TARGET_FILE:muw3>)
        set_tests_properties(mulib_muw3lib_loopback PROPERTIES TIMEOUT 30)
      endif()
      find_program(MULIB_OPENSSL_ORACLE openssl)
      if(MULIB_OPENSSL_ORACLE)
        add_test(NAME mulib_tls_interoperability
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tls_integration.py
            --muw3 $<TARGET_FILE:muw3> --x509 $<TARGET_FILE:mulib_test-x509>
            --signature $<TARGET_FILE:mulib_test-signature>)
        set_tests_properties(mulib_tls_interoperability PROPERTIES TIMEOUT 180)
      endif()
    endif()
    add_test(NAME mulib_readline_profiles_generated
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_librl.py --check)
    add_test(NAME mulib_archml_profiles_generated
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_archml.py --check)
    add_test(NAME mulib_unit_profiles_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_units.py --check)
    add_test(NAME mulib_ldocstr_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_ldocstr.py --check)
    add_test(NAME mulib_notation_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_notation.py --check)
  endif()
  add_executable(mulib_smoke tests/smoke.c)
  mulib_test_executable(mulib_smoke)
  mulib_register_test(mulib_smoke)

  # Compile the shared implementation TU once, while retaining separate clients.
  add_library(mulib_test_implementation OBJECT tests/implementation.c)
  mulib_test_executable(mulib_test_implementation)
  add_executable(mulib_functional $<TARGET_OBJECTS:mulib_test_implementation> tests/functional.c)
  mulib_test_executable(mulib_functional)
  mulib_register_test(mulib_functional)

  add_executable(mulib_composition $<TARGET_OBJECTS:mulib_test_implementation> tests/composition.c)
  mulib_test_executable(mulib_composition)
  mulib_register_test(mulib_composition)

  add_executable(mulib_extensions $<TARGET_OBJECTS:mulib_test_implementation> tests/extensions.c)
  mulib_test_executable(mulib_extensions)
  mulib_register_test(mulib_extensions)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(mulib_harness_implementation OBJECT tests/harness-kernels/implementation.c)
    mulib_test_executable(mulib_harness_implementation)
    foreach(test test_harness fuzz_unidiff test_rollback test_process_start)
      add_executable(mulib_${test} tests/harness-kernels/${test}.c
        $<TARGET_OBJECTS:mulib_harness_implementation>)
      target_compile_definitions(mulib_${test} PRIVATE _GNU_SOURCE _POSIX_C_SOURCE=200809L)
      mulib_test_executable(mulib_${test})
      mulib_register_test(mulib_${test})
    endforeach()
    target_link_options(mulib_test_rollback PRIVATE -Wl,--wrap=renameat,--wrap=fsync)
    target_link_options(mulib_test_process_start PRIVATE -Wl,--wrap=execvp)
  endif()

  add_executable(mulib_all_headers tests/all_headers.c)
  mulib_test_executable(mulib_all_headers)
  mulib_register_test(mulib_all_headers)

  add_executable(mulib_declarations tests/declarations.c)
  mulib_test_executable(mulib_declarations)
  mulib_register_test(mulib_declarations)

  add_executable(mulib_test_cdd tests/cdd-test.c tests/cdd-implementation.c)
  mulib_test_executable(mulib_test_cdd)
  mulib_register_test(mulib_test_cdd)

  add_library(mulib_secret_implementation OBJECT tests/secret/implementation.c)
  target_compile_definitions(mulib_secret_implementation PRIVATE _POSIX_C_SOURCE=200809L)
  mulib_test_executable(mulib_secret_implementation)
  add_executable(mulib_test_secret_native tests/secret/native.c
    $<TARGET_OBJECTS:mulib_secret_implementation>)
  mulib_test_executable(mulib_test_secret_native)
  mulib_register_test(mulib_test_secret_native)
  if(UNIX)
    add_executable(mulib_test_secret_broker tests/secret/test_secret_broker.c
      $<TARGET_OBJECTS:mulib_secret_implementation>)
    mulib_test_executable(mulib_test_secret_broker)
    mulib_register_test(mulib_test_secret_broker)
  endif()
  add_executable(mulib_secret_example examples/mulib-secret-kernels.c)
  mulib_test_executable(mulib_secret_example)
  mulib_register_test(mulib_secret_example)
  set_tests_properties(mulib_test_secret_native mulib_secret_example
    PROPERTIES SKIP_RETURN_CODE 77)
  if(MULIB_BUILD_SECRET_OPENSSL_TESTS)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    add_executable(mulib_test_secret_openssl tests/secret/openssl.c
      $<TARGET_OBJECTS:mulib_secret_implementation>)
    target_link_libraries(mulib_test_secret_openssl PRIVATE OpenSSL::Crypto)
    mulib_test_executable(mulib_test_secret_openssl)
    mulib_register_test(mulib_test_secret_openssl)
    add_executable(mulib_test_secret_openssl_vectors tests/test_secret_crypto_vectors.c)
    target_compile_definitions(mulib_test_secret_openssl_vectors PRIVATE MULIB_SECRET_TEST_OPENSSL)
    target_link_libraries(mulib_test_secret_openssl_vectors PRIVATE OpenSSL::Crypto)
    mulib_test_executable(mulib_test_secret_openssl_vectors)
    mulib_register_test(mulib_test_secret_openssl_vectors)
    add_executable(mulib_test_secret_openssl_credentials tests/test_secret_credential_vectors.c)
    target_compile_definitions(mulib_test_secret_openssl_credentials PRIVATE MULIB_TEST_SECRET_OPENSSL)
    target_link_libraries(mulib_test_secret_openssl_credentials PRIVATE OpenSSL::Crypto)
    mulib_test_executable(mulib_test_secret_openssl_credentials)
    mulib_register_test(mulib_test_secret_openssl_credentials)
    if(UNIX)
      add_executable(mulib_test_secret_storage tests/secret/storage-hardening.c)
      target_compile_definitions(mulib_test_secret_storage PRIVATE _POSIX_C_SOURCE=200809L)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
        target_compile_definitions(mulib_test_secret_storage PRIVATE _DEFAULT_SOURCE MULIB_TEST_STORE_FAULTS)
        target_link_options(mulib_test_secret_storage PRIVATE "-Wl,--wrap=fsync,--wrap=write,--wrap=read,--wrap=mlock")
      endif()
      target_link_libraries(mulib_test_secret_storage PRIVATE OpenSSL::Crypto)
      mulib_test_executable(mulib_test_secret_storage)
      mulib_register_test(mulib_test_secret_storage)
    endif()
  endif()

  add_executable(mulib_kernel_substrates examples/kernel-substrates.c)
  mulib_test_executable(mulib_kernel_substrates)
  mulib_register_test(mulib_kernel_substrates)

  add_executable(mulib_media_pipeline examples/media-pipeline.c)
  mulib_test_executable(mulib_media_pipeline)
  mulib_register_test(mulib_media_pipeline)
  add_executable(mulib_media_link tests/media-link-a.c tests/media-link-b.c)
  mulib_test_executable(mulib_media_link)
  mulib_register_test(mulib_media_link)
  add_executable(mulib_mcp_broadcast_example examples/modules/mcp-broadcast.c)
  mulib_test_executable(mulib_mcp_broadcast_example)
  mulib_register_test(mulib_mcp_broadcast_example)
  if(Python3_Interpreter_FOUND)
    add_test(NAME mulib_media_inventory
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/media_inventory.py" --check)
    add_test(NAME mulib_media_tools
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-media-tools.py")
    set_tests_properties(mulib_media_inventory mulib_media_tools PROPERTIES TIMEOUT 60)
    add_test(NAME mulib_termdb_tool COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_termdb_tool.py")
    set_tests_properties(mulib_termdb_tool PROPERTIES TIMEOUT 60)
    add_test(NAME mulib_kernelbind_inventory COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/extend-kernelbind.py" --check)
    add_test(NAME mulib_kernelbind_generator COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_extend_kernelbind.py")
    set_tests_properties(mulib_kernelbind_inventory mulib_kernelbind_generator PROPERTIES TIMEOUT 60)
  endif()

  file(GLOB focused_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/media-*-test.c")
  add_library(mulib_api_kernels_implementation OBJECT
    tests/api-kernels/implementation.c tests/api-kernels/dependencies.c)
  mulib_test_executable(mulib_api_kernels_implementation)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING
      AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|aarch64)$")
    # The worker must use the production syscall policy, not an ASan runtime.
    add_test(NAME mulib_test_sandbox
      COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
        "SANITIZE=$<BOOL:${MULIB_SANITIZERS}>"
        sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_sandbox.sh")
    set_tests_properties(mulib_test_sandbox PROPERTIES TIMEOUT 60)
  endif()
  foreach(source_file IN LISTS focused_tests)
    get_filename_component(test_name "${source_file}" NAME_WE)
    if(test_name MATCHES "^test_ssh_" AND NOT MULIB_BUILD_SSH_OPENSSL)
      continue() # OpenSSL-backed SSH kernels are opt-in.
    endif()
    if(test_name MATCHES "^test-(notebook-headless|wl-headless-private|x11-nested-live)$")
      continue() # dedicated multi-source fixtures or optional live Xvfb
    endif()
    if(test_name MATCHES "^test-(notebook-pty|x11-nested-wire)$"
        AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
      continue()
    endif()
    if(test_name STREQUAL "test_sandbox")
      continue() # requires separately built trusted-bootstrap fixtures
    endif()
    if(test_name MATCHES "^test_(parallel|shebang|sh_|mune_compile)")
      if(NOT UNIX)
        continue()
      endif()
      if(test_name STREQUAL "test_mune_compile")
        add_library("mulib_${test_name}" OBJECT "${source_file}")
        mulib_test_executable("mulib_${test_name}")
        target_compile_definitions("mulib_${test_name}" PRIVATE _POSIX_C_SOURCE=200809L)
        continue()
      endif()
    endif()
    add_executable("mulib_${test_name}" "${source_file}")
    mulib_test_executable("mulib_${test_name}")
    if(test_name MATCHES "^test_ssh_")
      target_sources("mulib_${test_name}" PRIVATE $<TARGET_OBJECTS:mulib_ssh_test_implementation>)
      target_link_libraries("mulib_${test_name}" PRIVATE OpenSSL::Crypto)
    endif()
    if(test_name MATCHES "^test-gfx-")
      target_compile_definitions("mulib_${test_name}" PRIVATE _POSIX_C_SOURCE=200809L)
      if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Untouched overlay helpers still use compact, single-line formatting.
        target_compile_options("mulib_${test_name}" PRIVATE -Wno-misleading-indentation)
      endif()
    endif()
    if(test_name STREQUAL "test-gfx-input")
      target_sources("mulib_${test_name}" PRIVATE
        tests/gfx-input-tu.c tests/gfx-input-unix.c tests/gfx-input-evdev.c)
    elseif(test_name STREQUAL "test-gfx-retgtk-core")
      target_sources("mulib_${test_name}" PRIVATE
        tests/gfx-retgtk-core-types-a.c tests/gfx-retgtk-core-types-b.c)
    elseif(test_name STREQUAL "test-gfx-retgtk-model-layout")
      target_sources("mulib_${test_name}" PRIVATE tests/gfx-retgtk-model-layout-peer.c)
    elseif(test_name STREQUAL "test_raytracing")
      target_sources("mulib_${test_name}" PRIVATE tests/raytracing-implementation.c)
    endif()
    if(test_name MATCHES "^test-api-(json-http|sse-client|websocket)$")
      target_sources("mulib_${test_name}" PRIVATE $<TARGET_OBJECTS:mulib_api_kernels_implementation>)
    endif()
    if(test_name MATCHES "^test_(parallel|shebang|sh_)")
      target_compile_definitions("mulib_${test_name}" PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    if(test_name STREQUAL "test-x11-nested-wire")
      target_compile_definitions("mulib_${test_name}" PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    if(test_name STREQUAL "test_android_sdk")
      target_include_directories("mulib_${test_name}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/android-sdk")
    endif()
    if(test_name STREQUAL "test_mune_runtime")
      set_target_properties("mulib_${test_name}" PROPERTIES ENABLE_EXPORTS ON)
    endif()
    mulib_register_test("mulib_${test_name}")
  endforeach()
  if(UNIX)
    add_test(NAME mulib_graphics_gate
      COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
        "SANITIZE=$<BOOL:${MULIB_SANITIZERS}>"
        sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/run-graphics.sh")
    set_tests_properties(mulib_graphics_gate PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 300)
  endif()
  if(Python3_Interpreter_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
      AND NOT CMAKE_CROSSCOMPILING AND CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    add_test(NAME mulib_notebook_desktop_headers
      COMMAND ${CMAKE_COMMAND} -E env "SANITIZE=$<BOOL:${MULIB_SANITIZERS}>"
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/run-notebook-desktop-headers.py"
        --cc "${CMAKE_C_COMPILER}")
    set_tests_properties(mulib_notebook_desktop_headers PROPERTIES TIMEOUT 60)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME mulib_notebook_headless
      COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
        "SANITIZE=$<BOOL:${MULIB_SANITIZERS}>"
        sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/run-notebook-headless.sh")
    set_tests_properties(mulib_notebook_headless PROPERTIES TIMEOUT 60)
    add_executable(mulib_test_wl_headless_private
      tests/test-wl-headless-private.c examples/wl-headless-private.c)
    target_compile_definitions(mulib_test_wl_headless_private PRIVATE _GNU_SOURCE)
    mulib_test_executable(mulib_test_wl_headless_private)
    mulib_register_test(mulib_test_wl_headless_private)
  endif()
  if(UNIX)
    foreach(name parallel shebang)
      add_executable("mulib_${name}_sh" "tests/${name}.c")
      mulib_test_executable("mulib_${name}_sh")
      target_compile_definitions("mulib_${name}_sh" PRIVATE _POSIX_C_SOURCE=200809L)
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_sh_cli")
    add_test(NAME mulib_sh_cli COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sh_cli.sh"
      $<TARGET_FILE:mulib_parallel_sh> $<TARGET_FILE:mulib_shebang_sh>
      "${CMAKE_CURRENT_BINARY_DIR}/test-work/mulib_sh_cli")
    set_tests_properties(mulib_sh_cli PROPERTIES TIMEOUT 60)
  endif()
  # Exercise actual C linkage, not a C++ recompilation of the implementations.
  include(CheckLanguage)
  check_language(CXX)
  if(CMAKE_CXX_COMPILER)
    enable_language(CXX)
    if(TARGET mulib_harness_implementation)
      add_executable(mulib_test_harness_cpp tests/harness-kernels/consumer.cpp
        $<TARGET_OBJECTS:mulib_harness_implementation>)
      set_target_properties(mulib_test_harness_cpp PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
      mulib_test_executable(mulib_test_harness_cpp)
      mulib_register_test(mulib_test_harness_cpp)
    endif()
    add_executable(mulib_test_secret_cpp_link tests/secret/native.cpp
      $<TARGET_OBJECTS:mulib_secret_implementation>)
    add_executable(mulib_test_secret_cpp_impl tests/secret/native.cpp)
    target_compile_definitions(mulib_test_secret_cpp_impl PRIVATE
      _POSIX_C_SOURCE=200809L MULIB_SECRET_TEST_CPP_IMPLEMENTATION)
    foreach(target mulib_test_secret_cpp_link mulib_test_secret_cpp_impl)
      set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
      mulib_test_executable(${target})
      mulib_register_test(${target})
      set_tests_properties(${target} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()

    add_executable(mulib_test_markup_cpp tests/markup-ir-consumer.cpp tests/fixtures/markup-ir-implementation.c)
    set_target_properties(mulib_test_markup_cpp PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    mulib_test_executable(mulib_test_markup_cpp)
    mulib_register_test(mulib_test_markup_cpp)

    add_executable(mulib_test_api_kernels_cpp tests/api-kernels/link.cpp
      $<TARGET_OBJECTS:mulib_api_kernels_implementation>)
    set_target_properties(mulib_test_api_kernels_cpp PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    mulib_test_executable(mulib_test_api_kernels_cpp)
    mulib_register_test(mulib_test_api_kernels_cpp)

    add_executable(mulib_test_hash_cpp tests/test_hash_cpp.cpp tests/hash_implementations.c)
    set_target_properties(mulib_test_hash_cpp PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    mulib_test_executable(mulib_test_hash_cpp)
    mulib_register_test(mulib_test_hash_cpp)

    add_library(mulib_network_implementation OBJECT tests/cpp_impl.c)
    mulib_test_executable(mulib_network_implementation)
    add_executable(mulib_test_network_cpp tests/cpp_consumer.cpp
      $<TARGET_OBJECTS:mulib_network_implementation>)
    set_target_properties(mulib_test_network_cpp PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    mulib_test_executable(mulib_test_network_cpp)
    mulib_register_test(mulib_test_network_cpp)

    add_library(mulib_pricing_implementation OBJECT tests/price_implementations.c)
    mulib_test_executable(mulib_pricing_implementation)
    add_executable(mulib_test_pricing_cpp tests/test_pricing_cpp.cpp
      $<TARGET_OBJECTS:mulib_pricing_implementation>)
    set_target_properties(mulib_test_pricing_cpp PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    mulib_test_executable(mulib_test_pricing_cpp)
    mulib_register_test(mulib_test_pricing_cpp)

    add_library(mulib_eda_implementation OBJECT tests/engineering_implementation.c)
    mulib_test_executable(mulib_eda_implementation)
    add_executable(mulib_test_cpp_link tests/test_cpp_link.cpp
      $<TARGET_OBJECTS:mulib_eda_implementation>)
    add_executable(mulib_test_all_headers_cpp tests/test_all_headers_cpp.cpp)
    foreach(target mulib_test_cpp_link mulib_test_all_headers_cpp)
      set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
      mulib_test_executable(${target})
      mulib_register_test(${target})
    endforeach()
  endif()
  if(Python3_Interpreter_FOUND)
    if(NOT CMAKE_CROSSCOMPILING AND CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
      set(secret_header_compilers --cc "${CMAKE_C_COMPILER}")
      if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        list(APPEND secret_header_compilers --cxx "${CMAKE_CXX_COMPILER}")
      endif()
      add_test(NAME mulib_test_secret_headers
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/secret/check-headers.py"
          ${secret_header_compilers})
      set_tests_properties(mulib_test_secret_headers PROPERTIES TIMEOUT 60)
    endif()
    if(UNIX AND NOT CMAKE_CROSSCOMPILING)
      add_executable(mulib_test_api_kernels_http
        tests/api-kernels/http-client.c $<TARGET_OBJECTS:mulib_api_kernels_implementation>)
      mulib_test_executable(mulib_test_api_kernels_http)
      add_test(NAME mulib_api_kernels_loopback
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_api_kernels_loopback.py
          --client $<TARGET_FILE:mulib_test_api_kernels_http>)
      set_tests_properties(mulib_api_kernels_loopback PROPERTIES TIMEOUT 20)
    endif()
    foreach(role client server)
      add_executable("mulib_mcp_${role}_example" "examples/modules/mcp-${role}.c")
      mulib_test_executable("mulib_mcp_${role}_example")
    endforeach()
    add_test(NAME mulib_mcp_stdio
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mcp_stdio.py
        --client $<TARGET_FILE:mulib_mcp_client_example>
        --server $<TARGET_FILE:mulib_mcp_server_example>)
    set_tests_properties(mulib_mcp_stdio PROPERTIES TIMEOUT 30)
    if(NOT CMAKE_CROSSCOMPILING)
      add_test(NAME mulib_mcp_http_loopback
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mcp_http.py
          --fixture $<TARGET_FILE:mulib_test_mcp_http>)
      set_tests_properties(mulib_mcp_http_loopback PROPERTIES TIMEOUT 30)
      if(UNIX)
        add_test(NAME mulib_opencode_loopback
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_opencode_loopback.py
            --client $<TARGET_FILE:mulib_test-opencode-client>)
        set_tests_properties(mulib_opencode_loopback PROPERTIES TIMEOUT 20)
      endif()
    endif()
    add_test(NAME mulib_compression_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_compression.py
        --codec $<TARGET_FILE:mulib_test_compression_codecs>)
    set_tests_properties(mulib_compression_interoperability PROPERTIES TIMEOUT 180)
    find_program(MULIB_PANDOC_EXECUTABLE NAMES pandoc)
    set(docstring_options)
    if(MULIB_PANDOC_EXECUTABLE)
      list(APPEND docstring_options --pandoc ${MULIB_PANDOC_EXECUTABLE})
    endif()
    add_test(NAME mulib_docstring_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_docstring.py
        --runtime $<TARGET_FILE:mulib_test_docstring>
        --library ${CMAKE_CURRENT_SOURCE_DIR}/library/ldocstr ${docstring_options})
    set_tests_properties(mulib_docstring_interoperability PROPERTIES TIMEOUT 180)
    find_program(MULIB_RSVG_EXECUTABLE NAMES rsvg-convert)
    find_program(MULIB_PDFLATEX_EXECUTABLE NAMES pdflatex)
    find_program(MULIB_LUALATEX_EXECUTABLE NAMES lualatex)
    set(notation_options)
    if(MULIB_RSVG_EXECUTABLE)
      list(APPEND notation_options --rsvg ${MULIB_RSVG_EXECUTABLE})
    endif()
    if(MULIB_PDFLATEX_EXECUTABLE)
      list(APPEND notation_options --pdflatex ${MULIB_PDFLATEX_EXECUTABLE})
    endif()
    if(MULIB_LUALATEX_EXECUTABLE)
      list(APPEND notation_options --lualatex ${MULIB_LUALATEX_EXECUTABLE})
    endif()
    add_test(NAME mulib_notation_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_notation.py
        --runtime $<TARGET_FILE:mulib_test_notation> ${notation_options})
    set_tests_properties(mulib_notation_interoperability PROPERTIES TIMEOUT 240)
  endif()
  find_program(MULIB_NODE_EXECUTABLE NAMES node nodejs)
  find_program(MULIB_CPIO_EXECUTABLE NAMES cpio)
  find_program(MULIB_CLANG_EXECUTABLE NAMES clang)
  find_program(MULIB_LLD_LINK_EXECUTABLE NAMES lld-link)
  if(Python3_Interpreter_FOUND AND MULIB_CPIO_EXECUTABLE AND MULIB_CLANG_EXECUTABLE AND MULIB_LLD_LINK_EXECUTABLE AND UNIX)
    add_test(NAME mulib_boot_formats_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_boot_formats.py
        --archive $<TARGET_FILE:mulib_test_cpio_init_shoebox>
        --efi $<TARGET_FILE:mulib_test_boot_standards>
        --cpio ${MULIB_CPIO_EXECUTABLE} --clang ${MULIB_CLANG_EXECUTABLE}
        --linker ${MULIB_LLD_LINK_EXECUTABLE})
    set_tests_properties(mulib_boot_formats_interoperability PROPERTIES TIMEOUT 60)
  endif()
  if(Python3_Interpreter_FOUND AND UNIX)
    add_test(NAME mulib_sqlite_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_sqlite.py
        --engine $<TARGET_FILE:mulib_test_sqlite>)
    set_tests_properties(mulib_sqlite_interoperability PROPERTIES TIMEOUT 180)
  endif()
  find_program(MULIB_FFMPEG_EXECUTABLE NAMES ffmpeg)
  find_program(MULIB_PDFTOTEXT_EXECUTABLE NAMES pdftotext)
  find_program(MULIB_PDFINFO_EXECUTABLE NAMES pdfinfo)
  if(Python3_Interpreter_FOUND)
    set(ebook_options)
    if(MULIB_PDFTOTEXT_EXECUTABLE)
      list(APPEND ebook_options --pdftotext ${MULIB_PDFTOTEXT_EXECUTABLE})
    endif()
    if(MULIB_PDFINFO_EXECUTABLE)
      list(APPEND ebook_options --pdfinfo ${MULIB_PDFINFO_EXECUTABLE})
    endif()
    add_test(NAME mulib_ebook_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_ebook_formats.py
        --codec $<TARGET_FILE:mulib_test_ebook_formats> ${ebook_options})
    set_tests_properties(mulib_ebook_interoperability PROPERTIES TIMEOUT 120)
  endif()
  find_program(MULIB_FLAC_EXECUTABLE NAMES flac)
  if(Python3_Interpreter_FOUND AND MULIB_FFMPEG_EXECUTABLE AND MULIB_FLAC_EXECUTABLE)
    add_test(NAME mulib_audio_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_audio.py
        --codec $<TARGET_FILE:mulib_test_audio_codecs>
        --ffmpeg ${MULIB_FFMPEG_EXECUTABLE} --flac ${MULIB_FLAC_EXECUTABLE})
    set_tests_properties(mulib_audio_interoperability PROPERTIES TIMEOUT 120)
  endif()
  find_program(MULIB_LLVM_MC_EXECUTABLE NAMES llvm-mc)
  if(Python3_Interpreter_FOUND AND MULIB_LLVM_MC_EXECUTABLE)
    add_test(NAME mulib_archml_encoding_oracle
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_archml.py
        --encoder $<TARGET_FILE:mulib_test_archml> --llvm-mc ${MULIB_LLVM_MC_EXECUTABLE})
    set_tests_properties(mulib_archml_encoding_oracle PROPERTIES TIMEOUT 180)
  endif()
  if(MULIB_NODE_EXECUTABLE)
    add_test(NAME mulib_machinebuild_js
      COMMAND ${MULIB_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_machinebuild.js
        $<TARGET_FILE:mulib_test_fst_machinebuild>)
    set_tests_properties(mulib_machinebuild_js PROPERTIES TIMEOUT 60)
  endif()
  find_program(MULIB_SYSTEMD_ANALYZE NAMES systemd-analyze)
  if(Python3_Interpreter_FOUND AND MULIB_SYSTEMD_ANALYZE)
    add_test(NAME mulib_daemon_systemd
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_daemon.py
        --emitter $<TARGET_FILE:mulib_test_daemon_fswatch> --validator ${MULIB_SYSTEMD_ANALYZE})
    set_tests_properties(mulib_daemon_systemd PROPERTIES TIMEOUT 60)
  endif()
  set_tests_properties(mulib_test_vmm_kvm PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: KVM")
  if(Python3_Interpreter_FOUND AND MULIB_GLSLANG_VALIDATOR)
    add_test(NAME mulib_shaderx_backends
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_graphics_shaders.py
        --emitter $<TARGET_FILE:mulib_test_graphics_languages>
        --validator ${MULIB_GLSLANG_VALIDATOR})
    set_tests_properties(mulib_shaderx_backends PROPERTIES TIMEOUT 180)
  endif()
  add_test(NAME mulib_readline_profiles
    COMMAND mulib_test_readline ${CMAKE_CURRENT_SOURCE_DIR}/library/librl)
  if(Python3_Interpreter_FOUND AND UNIX)
    add_test(NAME mulib_readline_native_io
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readline_io.py
        $<TARGET_FILE:mulib_test_readline>)
    set_tests_properties(mulib_readline_native_io PROPERTIES TIMEOUT 60)
    add_test(NAME mulib_prowse_native_io
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_prowse_io.py
        --protocol $<TARGET_FILE:mulib_test_prowse> --repl $<TARGET_FILE:mulib_test_prowse>)
    set_tests_properties(mulib_prowse_native_io PROPERTIES TIMEOUT 90)
  endif()
  if(Python3_Interpreter_FOUND AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(android_options)
    if(MULIB_SANITIZERS)
      list(APPEND android_options --sanitizers)
    endif()
    add_test(NAME mulib_android_interoperability
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_android.py
        --runtime $<TARGET_FILE:mulib_test_android_runtime> --abi $<TARGET_FILE:mulib_test_android_abi>
        --compiler ${CMAKE_C_COMPILER} ${android_options})
    set_tests_properties(mulib_android_interoperability PROPERTIES TIMEOUT 180)
    set(dynbox_options)
    if(MULIB_SANITIZERS)
      list(APPEND dynbox_options --sanitizers)
    endif()
    add_test(NAME mulib_dynbox_compiler_abi
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dynbox_abi.py
        --compiler ${CMAKE_C_COMPILER} ${dynbox_options})
    set_tests_properties(mulib_dynbox_compiler_abi PROPERTIES TIMEOUT 240)
    set(asdl_options)
    if(MULIB_SANITIZERS)
      list(APPEND asdl_options --sanitizers)
    endif()
    add_test(NAME mulib_asdl_generated
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_asdl.py
        --emitter $<TARGET_FILE:mulib_test_asdl> --compiler ${CMAKE_C_COMPILER} ${asdl_options})
    set_tests_properties(mulib_asdl_generated PROPERTIES TIMEOUT 60)
    add_test(NAME mulib_cassowary_reference
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_cassowary.py
        --solver $<TARGET_FILE:mulib_test_cassowary_solver>)
    set_tests_properties(mulib_cassowary_reference PROPERTIES TIMEOUT 60)
    add_test(NAME mulib_webidl_generated
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_webidl.py
        --emitter $<TARGET_FILE:mulib_test_webidl> --compiler ${CMAKE_C_COMPILER})
    set_tests_properties(mulib_webidl_generated PROPERTIES TIMEOUT 60)
    set(xll_options)
    if(MULIB_SANITIZERS)
      list(APPEND xll_options --sanitizers)
    endif()
    add_test(NAME mulib_xll_generated
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_xll.py
        --emitter $<TARGET_FILE:mulib_test_xll> --compiler ${CMAKE_C_COMPILER} ${xll_options})
    set_tests_properties(mulib_xll_generated PROPERTIES TIMEOUT 300)
    add_test(NAME mulib_metapp_reference
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_metapp.py
        --preprocessor $<TARGET_FILE:mulib_test_metapp> --compiler ${CMAKE_C_COMPILER})
    set_tests_properties(mulib_metapp_reference PROPERTIES TIMEOUT 180)
    set(microcc_options)
    if(MULIB_SANITIZERS)
      list(APPEND microcc_options --sanitizers)
    endif()
    add_test(NAME mulib_microcc_native
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_microcc_native.py
        --emitter $<TARGET_FILE:mulib_test_microcc> --compiler ${CMAKE_C_COMPILER} ${microcc_options})
    set_tests_properties(mulib_microcc_native PROPERTIES TIMEOUT 180)
    add_test(NAME mulib_isel_native
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_isel_native.py
        --emitter $<TARGET_FILE:mulib_test_isel> --compiler ${CMAKE_C_COMPILER} ${microcc_options})
    set_tests_properties(mulib_isel_native PROPERTIES TIMEOUT 180)
    set(mune_options)
    if(MULIB_SANITIZERS)
      list(APPEND mune_options --sanitizers)
    endif()
    add_test(NAME mulib_mune_runtime_backends
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_mune_runtime.py
        --runtime $<TARGET_FILE:mulib_test_mune_runtime> --compiler ${CMAKE_C_COMPILER} ${mune_options})
    set_tests_properties(mulib_mune_runtime_backends PROPERTIES TIMEOUT 300)
    foreach(emitter mulib_test_compiler_ir mulib_test_mir_flow)
      set(aot_options)
      if(MULIB_SANITIZERS)
        list(APPEND aot_options --sanitizers)
      endif()
      add_test(NAME "${emitter}_aot"
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_mir_aot.py
          --emitter $<TARGET_FILE:${emitter}> --compiler ${CMAKE_C_COMPILER}
          --include ${CMAKE_CURRENT_SOURCE_DIR}/include ${aot_options})
      set_tests_properties("${emitter}_aot" PROPERTIES TIMEOUT 180)
    endforeach()
  endif()
  find_program(MULIB_LUA54_EXECUTABLE NAMES lua5.4 lua54)
  if(Python3_Interpreter_FOUND AND MULIB_LUA54_EXECUTABLE)
    add_test(NAME mulib_mune_frontend_reference
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_mune_frontend.py
        --frontend $<TARGET_FILE:mulib_test_mune_frontend> --lua ${MULIB_LUA54_EXECUTABLE})
    set_tests_properties(mulib_mune_frontend_reference PROPERTIES TIMEOUT 180)
  endif()
  if(UNIX)
    add_executable(mulib_test_runtime_posix tests/test_runtime.c)
    target_compile_definitions(mulib_test_runtime_posix PRIVATE MULIB_MINISH_ENABLE_POSIX)
    mulib_test_executable(mulib_test_runtime_posix)
    mulib_register_test(mulib_test_runtime_posix)
  endif()
endif()

if(MULIB_BUILD_FUZZER)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "MULIB_BUILD_FUZZER requires Clang with libFuzzer support")
  endif()
  if(MULIB_BUILD_SSH_OPENSSL)
    foreach(parser transport_parse auth_channel_parse)
      add_executable(mulib_fuzz_ssh_${parser} tests/test_ssh_${parser}.c
        tests/ssh/wire.c tests/ssh/crypto.c tests/ssh/session.c)
      target_link_libraries(mulib_fuzz_ssh_${parser} PRIVATE OpenSSL::Crypto)
      target_compile_definitions(mulib_fuzz_ssh_${parser} PRIVATE MULIB_SSH_LIBFUZZER)
      mulib_test_executable(mulib_fuzz_ssh_${parser})
      target_compile_options(mulib_fuzz_ssh_${parser} PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
      target_link_options(mulib_fuzz_ssh_${parser} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
  endif()
  add_executable(mulib_fuzz_elvi_scrape tests/fuzz_elvi_scrape.c)
  mulib_test_executable(mulib_fuzz_elvi_scrape)
  target_compile_options(mulib_fuzz_elvi_scrape PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_elvi_scrape PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_markup_ir tests/fuzz-markup-ir.c)
  mulib_test_executable(mulib_fuzz_markup_ir)
  target_compile_options(mulib_fuzz_markup_ir PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_markup_ir PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_api_kernels tests/fuzz-api-kernels.c
    tests/api-kernels/implementation.c tests/api-kernels/dependencies.c)
  mulib_test_executable(mulib_fuzz_api_kernels)
  target_compile_options(mulib_fuzz_api_kernels PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_api_kernels PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_secret_parsers tests/secret/fuzz-parsers.c)
  mulib_test_executable(mulib_fuzz_secret_parsers)
  target_compile_options(mulib_fuzz_secret_parsers PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_secret_parsers PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_headers tests/fuzz_headers.c)
  mulib_test_executable(mulib_fuzz_headers)
  target_compile_options(mulib_fuzz_headers PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_headers PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_ebooks tests/fuzz_ebooks.c)
  mulib_test_executable(mulib_fuzz_ebooks)
  target_compile_options(mulib_fuzz_ebooks PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_ebooks PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_compression tests/fuzz_compression.c)
  mulib_test_executable(mulib_fuzz_compression)
  target_compile_options(mulib_fuzz_compression PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_compression PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_publishing_fonts tests/fuzz_publishing_fonts.c)
  mulib_test_executable(mulib_fuzz_publishing_fonts)
  target_compile_options(mulib_fuzz_publishing_fonts PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_publishing_fonts PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(mulib_fuzz_docir tests/fuzz_docir.c)
  mulib_test_executable(mulib_fuzz_docir)
  target_compile_options(mulib_fuzz_docir PRIVATE -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(mulib_fuzz_docir PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MULIB_BUILD_TESTS OR MULIB_BUILD_MODULE_EXAMPLES)
  add_executable(mulib_example_imtui_headless examples/imtui/headless.c)
  mulib_test_executable(mulib_example_imtui_headless)
  add_executable(mulib_example_imtui_composites examples/imtui/composites.c)
  mulib_test_executable(mulib_example_imtui_composites)
  if(UNIX)
    add_executable(mulib_example_imtui_terminal examples/imtui/terminal.c)
    mulib_test_executable(mulib_example_imtui_terminal)
  endif()
  add_executable(mulib_example_termscript_support examples/termscript/support.c)
  mulib_test_executable(mulib_example_termscript_support)
  add_executable(termscript library/termscript/main.c)
  target_compile_definitions(termscript PRIVATE
    MULIB_TERMSCRIPT_STDLIB="${CMAKE_CURRENT_SOURCE_DIR}/library/termscript"
    MULIB_XMACIR_TRS="${CMAKE_CURRENT_SOURCE_DIR}/library/xmacir-trs")
  mulib_test_executable(termscript)
  add_executable(mulib_example_cdd_pipeline examples/cdd-pipeline.c)
  mulib_test_executable(mulib_example_cdd_pipeline)
  add_executable(mulib_example_markup_ir examples/markup-ir.c)
  mulib_test_executable(mulib_example_markup_ir)
  if(MULIB_BUILD_TESTS)
    foreach(example hospitality_demo finpred_market)
      add_executable("mulib_${example}" "examples/${example}.c")
      mulib_test_executable("mulib_${example}")
      mulib_register_test("mulib_${example}")
    endforeach()
    add_executable(mulib_capasm_portable_example examples/modules/capasm.c)
    mulib_test_executable(mulib_capasm_portable_example)
    mulib_register_test(mulib_example_imtui_headless)
    mulib_register_test(mulib_example_imtui_composites)
    mulib_register_test(mulib_example_termscript_support)
    add_test(NAME mulib_termscript_counter COMMAND termscript "${CMAKE_CURRENT_SOURCE_DIR}/library/termscript/examples/counter.term")
    set_tests_properties(mulib_termscript_counter PROPERTIES PASS_REGULAR_EXPRESSION "10" TIMEOUT 30)
    if(UNIX AND Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING)
      add_test(NAME mulib_capasm_portable
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_capasm_portable.py"
          --example $<TARGET_FILE:mulib_capasm_portable_example> --cc "${CMAKE_C_COMPILER}"
          --sanitize "$<BOOL:${MULIB_SANITIZERS}>")
      set_tests_properties(mulib_capasm_portable PROPERTIES TIMEOUT 90)
      add_test(NAME mulib_xmacir_trs
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xmacir_trs.py")
      set_tests_properties(mulib_xmacir_trs PROPERTIES TIMEOUT 90)
      add_test(NAME mulib_termscript_cli
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_termscript_cli.py"
          --runner $<TARGET_FILE:termscript> --termdb-test $<TARGET_FILE:mulib_test_termdb>
          --compiler-test $<TARGET_FILE:mulib_test_termscript_compiler> --cc "${CMAKE_C_COMPILER}"
          --sanitize "$<BOOL:${MULIB_SANITIZERS}>")
      set_tests_properties(mulib_termscript_cli PROPERTIES TIMEOUT 120)
      add_test(NAME mulib_termscript_capasm_tools
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_termscript_capasm_tools.py"
          --runner $<TARGET_FILE:termscript>)
      set_tests_properties(mulib_termscript_capasm_tools PROPERTIES TIMEOUT 90)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
          CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND
          CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        add_test(NAME mulib_finpred_kernels
          COMMAND ${CMAKE_COMMAND} -E env "MULIB_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
            "SANITIZE=$<BOOL:${MULIB_SANITIZERS}>"
            sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/finpred-tests/run.sh")
        set_tests_properties(mulib_finpred_kernels PROPERTIES TIMEOUT 180)
      endif()
    endif()
    mulib_register_test(mulib_example_cdd_pipeline)
    mulib_register_test(mulib_example_markup_ir)
  endif()
endif()

if(MULIB_BUILD_MODULE_EXAMPLES)
  add_executable(mulib_example_fontshape_native examples/fontshape-native.c)
  mulib_test_executable(mulib_example_fontshape_native)
  if(MULIB_BUILD_TESTS)
    mulib_register_test(mulib_example_fontshape_native)
  endif()
  add_executable(mulib_example_yantlr_stdlib library/yantlr/example.c)
  target_link_libraries(mulib_example_yantlr_stdlib PRIVATE mulib_yantlr_stdlib)
  mulib_test_executable(mulib_example_yantlr_stdlib)
  if(MULIB_BUILD_TESTS)
    add_test(NAME mulib_example_yantlr_stdlib COMMAND mulib_example_yantlr_stdlib
      "${CMAKE_CURRENT_SOURCE_DIR}/library/yantlr/example.y1")
  endif()
  foreach(example_name quickstart dogfood calculator)
    add_executable("mulib_${example_name}" "examples/${example_name}.c")
    mulib_test_executable("mulib_${example_name}")
    if(MULIB_BUILD_TESTS)
      mulib_register_test("mulib_${example_name}")
    endif()
  endforeach()
  file(GLOB module_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/modules/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/mulib-*.c")
  foreach(source_file IN LISTS module_examples)
    get_filename_component(module_name "${source_file}" NAME_WLE)
    string(REGEX REPLACE "^mulib-" "" module_name "${module_name}")
    # Keep generated CMake target names valid for all module filenames.
    string(REGEX REPLACE "[^A-Za-z0-9_.+-]" "_" module_target "${module_name}")
    add_executable("mulib_example_${module_target}" "${source_file}")
    mulib_test_executable("mulib_example_${module_target}")
    if(module_name STREQUAL "sandbox-process" AND MULIB_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options("mulib_example_${module_target}" PRIVATE -fno-sanitize=address,undefined)
      target_link_options("mulib_example_${module_target}" PRIVATE -fno-sanitize=address,undefined)
    endif()
    if(MULIB_BUILD_TESTS)
      mulib_register_test("mulib_example_${module_target}")
    endif()
  endforeach()
endif()
