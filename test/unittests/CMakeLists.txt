cmake_minimum_required(VERSION 3.11)

include(FetchContent)
FetchContent_Declare(gtest
  QUIET
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)

# Prevent overriding the parent project's linker settings, for Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# GTest uses default label in switch which covers all enumeration values and
# Clang issues an error in this case. Let it be just a warning.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-Wno-error=covered-switch-default")
endif()

FetchContent_MakeAvailable(gtest)

include(GoogleTest)

include_directories(../../src)

set(UNITTESTS_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(SOURCE_DIR ${UNITTESTS_ROOT}/../../src)

function(add_lldb_mi_test NAME TEST)
  cmake_parse_arguments(
    ARG
    ""
    ""
    "SOURCES"
    ${ARGN}
  )

  set(SOURCES_RELATIVE "")
  foreach(SRC ${ARG_SOURCES})
    list(APPEND SOURCES_RELATIVE ${SOURCE_DIR}/${SRC})
  endforeach(SRC)

  add_executable(${NAME} ${TEST} ${SOURCES_RELATIVE})
  target_link_libraries(${NAME} gtest_main)
  set_target_properties(${NAME} PROPERTIES FOLDER "unittests")

  gtest_discover_tests(${NAME})
endfunction(add_lldb_mi_test)

add_subdirectory(arguments)
