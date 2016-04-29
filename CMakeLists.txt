cmake_minimum_required(VERSION 2.8)

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
endif()

#set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

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
	add_definitions(-std=c99 -fPIC)
endif()

include(CTest)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

include(GNUInstallDirs)

include(FindPkgConfig)

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
  set(GTCACA_LINK_LIBRARIES pthread)
endif()

set(GTCACA_INCLUDE_DIRS "${gtcaca-project_SOURCE_DIR}/src/include/" "${gtcaca-project_BINARY_DIR}/src/include")

set(GTCACA_LIBRARY gtcaca)
if(WIN32)
	set(GTCACA_LIBRARY "${gtcaca-project_BINARY_DIR}/src/${CMAKE_BUILD_TYPE}/gtcaca_static.lib")
endif(WIN32)
if(APPLE)
	set(GTCACA_LIBRARY "${gtcaca-project_BINARY_DIR}/src/libgtcaca.dylib")
endif(APPLE)

# Set our env variables
set(GTCACA_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/gtcaca/")
add_definitions(-DGTCACA_DATA_DIR="${GTCACA_DATA_DIR}")

add_subdirectory(src)
add_subdirectory(tests)
