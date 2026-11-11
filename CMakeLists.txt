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
  if(target STREQUAL "mulib_test_git_exec")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library/git")
  elseif(target STREQUAL "mulib_test_archml" OR target STREQUAL "mulib_test_isel")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library/archml")
  elseif(target STREQUAL "mulib_test_immgtk" OR target STREQUAL "mulib_example_immgtk")
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/library")
  elseif(target STREQUAL "mulib_publish_example")
    add_test(NAME ${target} COMMAND ${target} ".")
  else()
    add_test(NAME ${target} COMMAND ${target})
  endif()
  set_tests_properties(${target} PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/${target}" TIMEOUT 60)
endfunction()

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
    add_test(NAME mulib_tool_regressions
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tools.py)
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
    add_test(NAME mulib_lprowse_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_lprowse.py --check)
    add_test(NAME mulib_lmuw3_embedded
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_lmuw3.py --check)
    if(MULIB_BUILD_MUW3 AND UNIX)
      add_test(NAME mulib_muw3_integration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_muw3_integration.py
          --muw3 $<TARGET_FILE:muw3> --microchand $<TARGET_FILE:microchand>)
      set_tests_properties(mulib_muw3_integration PROPERTIES TIMEOUT 60)
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

  add_executable(mulib_all_headers tests/all_headers.c)
  mulib_test_executable(mulib_all_headers)
  mulib_register_test(mulib_all_headers)

  add_executable(mulib_declarations tests/declarations.c)
  mulib_test_executable(mulib_declarations)
  mulib_register_test(mulib_declarations)

  file(GLOB focused_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-*.c")
  foreach(source_file IN LISTS focused_tests)
    get_filename_component(test_name "${source_file}" NAME_WE)
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
    if(test_name MATCHES "^test_(parallel|shebang|sh_)")
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

    add_library(mulib_eda_implementation OBJECT tests/amb3r_implementation.c)
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
  find_program(MULIB_GLSLANG_VALIDATOR NAMES glslangValidator)
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
    if(MULIB_BUILD_TESTS)
      mulib_register_test("mulib_example_${module_target}")
    endif()
  endforeach()
endif()
