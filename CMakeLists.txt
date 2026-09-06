cmake_minimum_required(VERSION 3.16)
project(ximodem C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# The Arduino .ino unity build cannot satisfy -Wall/-Werror. When ximodem is
# built standalone this is the whole story; when it is add_subdirectory'd into a
# stricter parent (e.g. the x16 emulator) these flags come last and win.
#
# -funsigned-char: the vendored Zimodem firmware targets the ESP32, where `char`
# is unsigned. It treats bare `char` as a raw byte throughout (Telnet IAC / PETSCII
# translation: `if(*c == 255)`, `*c == TELNET_IAC`, petToAsc high bytes, ...). On
# x86-64 Linux `char` is signed, so 0xFF sign-extends to -1 and every one of those
# comparisons silently fails -- handleAsciiIAC() becomes a no-op and Telnet IAC IAC
# escapes are never de-doubled, corrupting binary (YMODEM) downloads. Matching the
# ESP32's char signedness restores the semantics the firmware was written against.
set(XIMODEM_RELAX_FLAGS -g -O1 -fno-omit-frame-pointer -w -Wno-error
                        -fpermissive -Wno-write-strings -Wno-narrowing
                        -funsigned-char)
add_compile_options(${XIMODEM_RELAX_FLAGS})

find_package(Threads REQUIRED)

# --- reconstruct the sketch (external/ + patches/) then regenerate the unity TU ---
# scripts/ holds the build-critical generators (committed); tools/ holds the
# local-only test harnesses (gitignored).
# patch_firmware.py: external/zimodem @ external/COMMIT + patches/* -> build/vendor/firmware
# gen_unity.py:      build/vendor/firmware -> src/generated/{unity.cpp,xi_prototypes.h}
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_firmware.py
  RESULT_VARIABLE VENDOR_RC
  OUTPUT_VARIABLE VENDOR_OUT
  ERROR_VARIABLE  VENDOR_ERR)
message(STATUS "patch_firmware.py: ${VENDOR_OUT}")
if(NOT VENDOR_RC EQUAL 0)
  message(FATAL_ERROR "patch_firmware.py failed:\n${VENDOR_ERR}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_unity.py
  RESULT_VARIABLE GEN_RC
  OUTPUT_VARIABLE GEN_OUT)
message(STATUS "gen_unity.py: ${GEN_OUT}")
if(NOT GEN_RC EQUAL 0)
  message(FATAL_ERROR "gen_unity.py failed")
endif()

add_custom_target(gen_unity ALL
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_firmware.py
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_unity.py
  BYPRODUCTS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/unity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/xi_prototypes.h
  COMMENT "Re-applying Zimodem patches (external/ + patches/) and regenerating the unity build")

set(COMPAT_SRC
  compat/core.cpp
  compat/Print.cpp
  compat/Stream.cpp
  compat/WString.cpp
  compat/IPAddress.cpp
  compat/HardwareSerial.cpp
  compat/FS.cpp
  compat/WiFi.cpp
  compat/xi_platform.cpp
  compat/tls.cpp
  compat/ping.cpp
  compat/stubs.cpp)

add_library(ximodem SHARED
  src/ximodem.cpp
  src/generated/unity.cpp
  ${COMPAT_SRC})
add_dependencies(ximodem gen_unity)
target_compile_options(ximodem PRIVATE ${XIMODEM_RELAX_FLAGS})

target_include_directories(ximodem
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat
          ${CMAKE_CURRENT_SOURCE_DIR}/build/vendor/firmware
          ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)

# Export ONLY the XIMODEM_API-tagged C entry points (see include/ximodem.h).
# WINDOWS_EXPORT_ALL_SYMBOLS re-exports everything the DLL pulls in -- including
# libstdc++ / libgcc / CRT symbols -- and the host .exe then auto-imports those
# from libximodem.dll instead of from libstdc++-6.dll, creating cross-module data
# relocs that fail as "32-bit pseudo relocation out of range" when the DLLs load
# far apart. Explicit exports fix that; all runtime libs stay dynamic.
target_compile_definitions(ximodem PRIVATE XIMODEM_BUILD)
set_target_properties(ximodem PROPERTIES
  C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
  WINDOWS_EXPORT_ALL_SYMBOLS OFF)

target_link_libraries(ximodem PRIVATE Threads::Threads)

if(WIN32)
  target_link_libraries(ximodem PRIVATE ws2_32 iphlpapi)
  target_link_options(ximodem PRIVATE -Wl,--exclude-all-symbols)
  # Diagnostic: turn any leftover data auto-import into a build-time link error
  # (naming the symbol) instead of a runtime pseudo-reloc crash.
  option(XIMODEM_STRICT_RELOC "Fail the link on any runtime pseudo-relocation" OFF)
  if(XIMODEM_STRICT_RELOC)
    target_link_options(ximodem PRIVATE -Wl,--disable-runtime-pseudo-reloc)
  endif()
endif()

# --- TLS (WiFiClientSecure) via OpenSSL -- optional -----------------------
# With OpenSSL: real TLS. Without: WiFiClientSecure falls back to a plaintext
# connect (upstream Zimodem calls setInsecure() anyway). Keeps the emulator
# build from hard-failing on machines without libssl-dev.
option(XIMODEM_NO_TLS "Build WiFiClientSecure as a plaintext stub" OFF)
# Link OpenSSL as DLLs, never merge the static archives into libximodem.
set(OPENSSL_USE_STATIC_LIBS OFF)
if(NOT XIMODEM_NO_TLS)
  find_package(OpenSSL)
endif()
if(OPENSSL_FOUND AND NOT XIMODEM_NO_TLS)
  message(STATUS "ximodem TLS: OpenSSL ${OPENSSL_VERSION}")
  target_compile_definitions(ximodem PRIVATE XIMODEM_HAVE_OPENSSL)
  target_link_libraries(ximodem PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
  message(STATUS "ximodem TLS: OpenSSL not used -- WiFiClientSecure is plaintext")
endif()

# --- SSH client (INCLUDE_SSH) via libssh2 ---------------------------------
# Preference order:
#   1. a real dev package (CMake config, then pkg-config) -- use ITS <libssh2.h>
#      so libssh2_socket_t etc. match the linked lib (needed on Windows / MSYS2).
#   2. fallback (CI sandbox): the vendored public header + a bare runtime lib.
# XIMODEM_NO_SSH=ON drops the SSH client entirely.
option(XIMODEM_NO_SSH "Build without the Zimodem SSH client" OFF)
# Manual override: -DLIBSSH2_INCLUDE_DIR=... -DLIBSSH2_LIBRARY=...
set(LIBSSH2_INCLUDE_DIR "" CACHE PATH   "libssh2 header directory (optional)")
set(LIBSSH2_LIBRARY     "" CACHE FILEPATH "libssh2 import/shared library (optional)")
if(XIMODEM_NO_SSH)
  target_compile_definitions(ximodem PRIVATE XIMODEM_NO_SSH)
else()
  find_package(Libssh2 CONFIG QUIET)
  if(NOT Libssh2_FOUND AND NOT (LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY))
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(LIBSSH2 IMPORTED_TARGET QUIET libssh2)
    endif()
  endif()
  if(LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY)
    message(STATUS "ximodem SSH: libssh2 (manual: ${LIBSSH2_LIBRARY})")
    target_include_directories(ximodem PRIVATE ${LIBSSH2_INCLUDE_DIR})
    target_link_libraries(ximodem PRIVATE ${LIBSSH2_LIBRARY})
    target_compile_definitions(ximodem PRIVATE XIMODEM_SYSTEM_LIBSSH2)
  elseif(Libssh2_FOUND)
    message(STATUS "ximodem SSH: system libssh2 (CMake config)")
    target_link_libraries(ximodem PRIVATE Libssh2::libssh2)
    target_compile_definitions(ximodem PRIVATE XIMODEM_SYSTEM_LIBSSH2)
  elseif(LIBSSH2_FOUND)
    message(STATUS "ximodem SSH: system libssh2 (pkg-config ${LIBSSH2_VERSION})")
    target_link_libraries(ximodem PRIVATE PkgConfig::LIBSSH2)
    target_compile_definitions(ximodem PRIVATE XIMODEM_SYSTEM_LIBSSH2)
  else()
    find_library(LIBSSH2_LIB NAMES ssh2 libssh2.so.1
                 PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
    if(NOT LIBSSH2_LIB)
      find_file(LIBSSH2_SO libssh2.so.1
                PATHS /usr/lib/x86_64-linux-gnu /usr/lib /lib/x86_64-linux-gnu)
      set(LIBSSH2_LIB ${LIBSSH2_SO})
    endif()
    if(LIBSSH2_LIB)
      message(STATUS "ximodem SSH: vendored header + ${LIBSSH2_LIB}")
      target_link_libraries(ximodem PRIVATE ${LIBSSH2_LIB})
    else()
      message(WARNING "ximodem SSH: libssh2 not found; building with XIMODEM_NO_SSH")
      target_compile_definitions(ximodem PRIVATE XIMODEM_NO_SSH)
    endif()
  endif()
endif()

# --- CLI tools: only when ximodem is the top-level project (standalone build)
# AND the local-only tools/ tree is present (it is gitignored -- a code-only
# checkout builds just the library).
# smoke is portable (no sockets/threads in the harness). dialtest/sigtest/flowtest
# stand up their own loopback TCP servers and pump threads, and pty_bridge needs a
# PTY -- all POSIX-only in the harness (the library under test stays portable).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
   AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/smoke.c)
  set(_xi_tools smoke:smoke baudtest:baudtest nettest:nettest)
  if(NOT WIN32)
    list(APPEND _xi_tools dialtest:dialtest pty:pty_bridge sigtest:sigtest
                         flowtest:flowtest ratetest:ratetest)
  endif()
  foreach(tool ${_xi_tools})
    string(REPLACE ":" ";" _t ${tool})
    list(GET _t 0 _name)
    list(GET _t 1 _src)
    add_executable(ximodem-${_name} tools/${_src}.c)
    target_include_directories(ximodem-${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(ximodem-${_name} PRIVATE ximodem Threads::Threads)
    if(WIN32)
      target_link_libraries(ximodem-${_name} PRIVATE ws2_32)
    endif()
  endforeach()
endif()

# Standalone build: remind where the shared library has to end up. An embedder
# that links libximodem resolves it via $ORIGIN, so the .so must sit in the same
# directory as the executable that loads it -- or in a system lib dir. (When a
# parent project builds ximodem as a subdirectory this is handled there.)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT WIN32)
  add_custom_command(TARGET ximodem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
      "  note: copy libximodem${CMAKE_SHARED_LIBRARY_SUFFIX} next to the executable that loads it"
    COMMAND ${CMAKE_COMMAND} -E echo
      "  (resolved via \$ORIGIN), or install it to a system library directory."
    VERBATIM)
endif()
