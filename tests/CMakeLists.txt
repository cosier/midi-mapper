cmake_minimum_required(VERSION 3.6)
project(${lib}-tests)

# This project uses C11 Standard
set( CMAKE_C_STANDARD 11 )

# Treat all warnings as intense errors.
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

file(GLOB_RECURSE HELPER_LIB helpers/*.c helpers/*.h)

# message ( FATAL_ERROR "HELPER_SRC: ${HELPER_LIB}" )
include_directories(
  "${DM_ROOT}/tests"
  "${DM_ROOT}/src")

set(lib ${APP_LIB_NAME})
add_library(test_helper STATIC ${HELPER_LIB})
# Test Executable that utilises the library.

add_executable(test-utils test_utils.c)
add_executable(test-key-maps test_key_maps.c)
add_executable(test-key-nodes test_key_nodes.c)
add_executable(test-note-parser test_note_parser.c)


# Link up main executable with our Library: libdimappiox
target_link_libraries( test-utils ${lib} test_helper)
target_link_libraries( test-key-maps ${lib} test_helper)
target_link_libraries( test-key-nodes ${lib} test_helper)
target_link_libraries( test-note-parser ${lib} test_helper)

add_test(NAME test-utils COMMAND test-utils)
add_test(NAME test-key-maps COMMAND test-key-maps)
add_test(NAME test-key-nodes COMMAND test-key-nodes)
add_test(NAME test-note-parser COMMAND test-note-parser)

