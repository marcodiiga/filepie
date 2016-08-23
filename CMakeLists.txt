cmake_minimum_required (VERSION 3.1)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_FLAGS "-Wall -pedantic -Wextra")

project (filepie)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
find_package (Curses REQUIRED)

include_directories (${CURSES_INCLUDE_DIR})

add_executable (filepie filepie.cpp)
target_link_libraries (filepie ${CURSES_LIBRARIES})

install (TARGETS filepie RUNTIME DESTINATION /usr/bin/)
