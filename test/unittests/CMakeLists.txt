enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

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
  target_link_libraries(${NAME} GTest::GTest GTest::Main)
  set_target_properties(${NAME} PROPERTIES FOLDER "unittests")

  add_test(unittests ${NAME})
endfunction(add_lldb_mi_test)

add_subdirectory(arguments)
