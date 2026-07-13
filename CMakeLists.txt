cmake_minimum_required(VERSION 3.20)

# Detect the actual macOS SDK path before project() initialises the toolchain,
# so CMake never warns about a stale/missing CMAKE_OSX_SYSROOT value.
if(APPLE)
  execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE _detected_sysroot
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_detected_sysroot)
    set(CMAKE_OSX_SYSROOT "${_detected_sysroot}" CACHE PATH "macOS SDK path" FORCE)
  endif()
endif()

project(gtcaca-project)

message("CMake system name: ${CMAKE_SYSTEM_NAME}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
message(FATAL_ERROR "CMake generation is not allowed within the source directory!
Remove the CMakeCache.txt file and try again from another folder, e.g.:

   rm CMakeCache.txt
   mkdir build
   cd build
   cmake ..
")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${gtcaca-project_SOURCE_DIR}/cmake)

include(GNUInstallDirs)

if(APPLE)
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/src")
endif()

#set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

set(GTCACA_VERSION "0.0.0")

include(FindPkgConfig)
configure_file("${gtcaca-project_SOURCE_DIR}/gtcaca.pc.cmake" "${gtcaca-project_BINARY_DIR}/gtcaca.pc")
message("pkg config path:${PKG_CONFIG_PATH}")
install(FILES "${gtcaca-project_BINARY_DIR}/gtcaca.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig" COMPONENT Headers)

if (DEBUG_MODE)
add_definitions(-DGTCACA_DEBUG=1)
set(CMAKE_BUILD_TYPE "Debug")
        if(NOT WIN32)
                add_definitions(-g)
        endif()
else()
        if(NOT WIN32)
                add_definitions(-O3)
        endif()
endif()
if (NOT WIN32)
	# gnu99, not c99: strict ISO C99 makes glibc hide the POSIX functions this
	# library actually uses (strdup, usleep, strcasecmp). GCC then implicitly
	# declares strdup as returning int, which TRUNCATES THE POINTER TO 32 BITS on
	# x86-64 — "makes pointer from integer without a cast" — corrupting every
	# strdup'd string. gnu99 exposes the POSIX declarations, so they return char*.
	add_definitions(-std=gnu99 -fPIC)
endif()

# That truncation shipped as a segfault on Linux because it was only a warning:
# an implicitly declared function defaults to returning int, so any missing
# prototype for a pointer-returning libc call silently corrupts the pointer on
# any 64-bit target. Make both halves of that hard errors — they are never
# benign in this codebase, and the compiler catches them for free.
# (GCC 14+ and Clang 15+ already error by default; this keeps older toolchains
# from reintroducing it.)
add_compile_options(
	-Werror=implicit-function-declaration
	-Werror=int-conversion
)

include(CTest)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

include(GNUInstallDirs)

include(TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN)
if (${BIGENDIAN})
  add_definitions(-DHAVE_LITTLE_ENDIAN=0)
  add_definitions(-DHAVE_BIG_ENDIAN=1)
else()
  add_definitions(-DHAVE_LITTLE_ENDIAN=1)
  add_definitions(-DHAVE_BIG_ENDIAN=0)
endif()

pkg_check_modules(LIBCACA REQUIRED caca)
message("libcaca link library: ${LIBCACA_LDFLAGS}")

# Oniguruma (optional): enables TextMate grammar (.tmLanguage.json) colourization.
pkg_check_modules(ONIG oniguruma)
if(ONIG_FOUND)
  message("oniguruma found: ${ONIG_LDFLAGS}")
  add_compile_definitions(GTCACA_HAVE_ONIG=1)
endif()

message("CMake system: ${CMAKE_SYSTEM_NAME}")
# Mac OS Specifics
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_definitions(-DMACOS=1)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "WIN32")
  add_definitions(-DWIN32=1)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_definitions(-DLINUX=1)
  set(GTCACA_LINK_LIBRARIES pthread m)
endif()

set(GTCACA_INCLUDE_DIRS "${gtcaca-project_SOURCE_DIR}/src/include/" "${gtcaca-project_BINARY_DIR}/src/include")

# Link by target name: a raw path would give make no gtcaca -> consumer edge,
# so parallel builds can start demo/apps before the library exists.
set(GTCACA_LIBRARY gtcaca)
if(WIN32)
	set(GTCACA_LIBRARY "${gtcaca-project_BINARY_DIR}/src/${CMAKE_BUILD_TYPE}/gtcaca_static.lib")
endif(WIN32)

# Set our env variables
set(GTCACA_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/gtcaca/")
add_definitions(-DGTCACA_DATA_DIR="${GTCACA_DATA_DIR}")

add_subdirectory(src)
add_subdirectory(demo)
add_subdirectory(apps)

# Theme data (so installed apps find their default theme rather than falling
# back to the built-in defaults with a warning).
install(FILES "${gtcaca-project_SOURCE_DIR}/themes/default"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gtcaca/themes" COMPONENT Applications)

# ── packaging: `make package` ────────────────────────────────────────────────
#   macOS : a productbuild .pkg INSTALLER that installs gtcaca + the apps under
#           /usr/local (so cacamacs/cacasheet/rfmind/rtodo are on $PATH).
#   else  : a .tar.gz of the same tree.
set(CPACK_PACKAGE_NAME "gtcaca")
set(CPACK_PACKAGE_VENDOR "gtcaca")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "gtcaca libcaca TUI toolkit and apps (cacamacs, cacasheet, rfmind, rtodo)")
set(CPACK_PACKAGE_VERSION "${GTCACA_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "gtcaca-${GTCACA_VERSION}-${CMAKE_SYSTEM_NAME}")
# install everything in one shot rather than offering per-component choices
set(CPACK_COMPONENTS_GROUPING "ALL_COMPONENTS_IN_ONE")
if(APPLE)
  set(CPACK_GENERATOR "productbuild")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
