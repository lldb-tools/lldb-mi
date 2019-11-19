cmake_minimum_required(VERSION 3.4.3)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

project(lldb-mi)


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

add_subdirectory(src)

