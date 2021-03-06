cmake_minimum_required(VERSION 3.0)
project(HTMLSPLIT)

set(HTMLSPLIT_VERSION "0.0.1")
set(CMAKE_MODULE_PATH ${HTMLSPLIT_SOURCE_DIR}/cmake/modules)

########################################
# Extra flags

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")

add_definitions("-D_POSIX_C_SOURCE=200112L")

########################################
# Dependencies

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
add_definitions(${LIBXML2_DEFINITIONS})

########################################
# Source files

configure_file("src/config.h" "${HTMLSPLIT_BINARY_DIR}/htmlsplit_config.h")
file(GLOB_RECURSE htmlsplit_sources
  "src/*.c"
  "src/*.h")

include_directories("${HTMLSPLIT_SOURCE_DIR}/src")
include_directories("${HTMLSPLIT_BINARY_DIR}")

########################################
# Targets

add_executable(htmlsplit ${htmlsplit_sources} ${HTMLSPLIT_BINARY_DIR}/htmlsplit_config.h)
target_link_libraries(htmlsplit ${LIBXML2_LIBRARIES})

########################################
# Installation information

install(TARGETS htmlsplit
  DESTINATION bin)
install(FILES "${HTMLSPLIT_SOURCE_DIR}/doc/man/htmlsplit.1"
  DESTINATION share/man/man1/)
