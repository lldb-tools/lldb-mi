cmake_minimum_required(VERSION 3.4.3)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

project(lldb-mi)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (USE_LLDB_FRAMEWORK AND !APPLE)
  message(FATAL_ERROR "USE_LLDB_FRAMEWORK is only avaliable on Darwin")
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(HandleLLVMStdlib)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
if(LLVM_BUILD_MAIN_SRC_DIR)
  include_directories(${LLVM_BUILD_MAIN_SRC_DIR}/../lldb/include)
  include_directories(${LLVM_BUILD_BINARY_DIR}/tools/lldb/include)
endif()

add_definitions(${LLVM_DEFINITIONS})

if (NOT LLVM_ENABLE_EH)
  if(LLVM_COMPILER_IS_GCC_COMPATIBLE)
    add_compile_options("-fno-exceptions")
  elseif(MSVC)
    add_compile_options("/EHs-c-")
    add_definitions("-D_HAS_EXCEPTIONS=0")
  endif()
endif()

if(MSVC)
  #Disable warning "multiple copy constructors specified".
  add_compile_options("/wd4521")
endif()

# https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
# use, i.e. don't skip, the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH FALSE CACHE BOOL "")
# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE CACHE BOOL "")
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" CACHE STRING "")
# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE CACHE BOOL "")
# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" CACHE STRING "")
ENDIF("${isSystemDir}" STREQUAL "-1")

add_subdirectory(src)

SET(INCLUDE_TESTS CACHE BOOL FALSE)
if (INCLUDE_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()
