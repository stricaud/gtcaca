cmake_minimum_required(VERSION 3.20)

# Detect the macOS SDK before project() so CMake never warns about a stale
# CMAKE_OSX_SYSROOT (mirrors the top-level project's behaviour).
if(APPLE)
  execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE _detected_sysroot
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_detected_sysroot)
    set(CMAKE_OSX_SYSROOT "${_detected_sysroot}" CACHE PATH "macOS SDK path" FORCE)
  endif()
endif()

project(gtcaca_python LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Repo root is three levels up: src/bindings/python -> repo root.
get_filename_component(GTCACA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(GTCACA_SRC_DIR "${GTCACA_ROOT}/src")
set(GTCACA_INCLUDE_DIR "${GTCACA_SRC_DIR}/include")

# --- dependencies ----------------------------------------------------------
find_package(pybind11 CONFIG REQUIRED)

if(WIN32)
  # No pkg-config on Windows: locate the libcaca that build-libcaca-win.sh
  # installed. CACA_ROOT (env or cache var) points at its install prefix.
  # CACA_ROOT (env or cache) or the CI default location C:/caca-dist.
  set(CACA_ROOT "$ENV{CACA_ROOT}" CACHE PATH "libcaca install prefix (Windows)")
  find_path(LIBCACA_INCLUDE_DIRS NAMES caca.h
    HINTS "${CACA_ROOT}/include" "${CACA_ROOT}" "C:/caca-dist/include")
  find_library(LIBCACA_LIBRARIES NAMES caca libcaca
    HINTS "${CACA_ROOT}/lib" "${CACA_ROOT}" "C:/caca-dist/lib")
  if(NOT LIBCACA_INCLUDE_DIRS OR NOT LIBCACA_LIBRARIES)
    message(FATAL_ERROR "libcaca not found. Set CACA_ROOT to its install prefix "
                        "(got CACA_ROOT='${CACA_ROOT}').")
  endif()
  set(LIBCACA_LIBRARY_DIRS "")
  message(STATUS "libcaca (Windows): ${LIBCACA_LIBRARIES}")
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBCACA REQUIRED caca)
endif()

# NOTE: oniguruma is deliberately NOT used here. It only enables TextMate-grammar
# colourization in the editor widget (not exposed by these bindings). Linking it
# would add a needless runtime dependency to the wheel — and on macOS a Homebrew
# libonig targets a newer OS than the wheel, breaking delocate.

# --- gtcaca C sources ------------------------------------------------------
# Compile the library sources straight into the extension so the wheel is
# self-contained (no separately-installed libgtcaca needed). Mirrors the list
# in src/CMakeLists.txt; kept explicit rather than globbed so a new source file
# doesn't silently change the build.
set(GTCACA_C_SOURCES
  main.c iniparse.c log.c
  application.c button.c checkbox.c combobox.c entry.c label.c menu.c
  progressbar.c radiobutton.c statusbar.c textlist.c textview.c theme.c
  widget.c window.c image.c spinner.c scale.c spinbutton.c switch.c frame.c
  separator.c expander.c box.c
  editor.c editor_lang.c editor_autoc.c editor_view.c editor_json.c
  editor_tm.c editor_search.c editor_ops.c
  json.c sparkline.c gauge.c barchart.c tree.c table.c map.c tabs.c mindmap.c
  segdisplay.c linechart.c hexview.c calendar.c dialog.c filechooser.c
  custom.c colordialog.c)
list(TRANSFORM GTCACA_C_SOURCES PREPEND "${GTCACA_SRC_DIR}/")

pybind11_add_module(_gtcaca MODULE
  gtcaca_module.cpp
  ${GTCACA_C_SOURCES})

target_include_directories(_gtcaca PRIVATE
  ${GTCACA_INCLUDE_DIR}
  ${LIBCACA_INCLUDE_DIRS})

target_link_libraries(_gtcaca PRIVATE ${LIBCACA_LIBRARIES})
target_link_directories(_gtcaca PRIVATE ${LIBCACA_LIBRARY_DIRS})

# Endianness + platform defines the C sources expect (see top-level CMakeLists).
include(TestBigEndian)
test_big_endian(GTCACA_BIGENDIAN)
if(GTCACA_BIGENDIAN)
  target_compile_definitions(_gtcaca PRIVATE HAVE_LITTLE_ENDIAN=0 HAVE_BIG_ENDIAN=1)
else()
  target_compile_definitions(_gtcaca PRIVATE HAVE_LITTLE_ENDIAN=1 HAVE_BIG_ENDIAN=0)
endif()

if(APPLE)
  target_compile_definitions(_gtcaca PRIVATE MACOS=1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_compile_definitions(_gtcaca PRIVATE LINUX=1)
  target_link_libraries(_gtcaca PRIVATE pthread m)
elseif(WIN32)
  target_compile_definitions(_gtcaca PRIVATE WIN32=1)
  if(MSVC)
    # The C sources use POSIX names (strdup, snprintf, isatty via the compat
    # shim). Quiet MSVC's deprecation/secure-CRT nags and select a modern C.
    target_compile_definitions(_gtcaca PRIVATE
      _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_options(_gtcaca PRIVATE /std:c11)
  endif()
endif()

# Default data dir for themes (no trailing colourization issues without onig).
target_compile_definitions(_gtcaca PRIVATE
  GTCACA_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/gtcaca/")

# The C sources are C99 but use POSIX APIs (strdup, usleep). Use gnu99, NOT
# strict c99: strict -std=c99 defines __STRICT_ANSI__, which makes glibc hide
# those POSIX functions, and GCC 14 then turns the implicit declarations into
# hard errors. (macOS libc exposes them regardless, which is why c99 built
# locally.) MSVC selects its C standard differently (/std:c11 above).
if(NOT MSVC)
  set_source_files_properties(${GTCACA_C_SOURCES} PROPERTIES
    COMPILE_FLAGS "-std=gnu99")
endif()

# Place the built module next to the Python package sources when building
# in-tree (so `import gtcaca` works from this directory without installing).
install(TARGETS _gtcaca DESTINATION gtcaca)
