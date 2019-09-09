cmake_minimum_required (VERSION 3.1)
project (MiniDeen)
add_library(MiniDeen SHARED avs_interface.cpp vs_interface.cpp version.rc)

find_package(Git)
execute_process(COMMAND ${GIT_EXECUTABLE} describe --first-parent --tags --always OUTPUT_VARIABLE GIT_REPO_VERSION)
string(REGEX REPLACE "(r[0-9]+).*" "\\1" VERSION ${GIT_REPO_VERSION})

configure_file (
  "${PROJECT_SOURCE_DIR}/version.hpp.in"
  "${PROJECT_SOURCE_DIR}/version.hpp"
)
configure_file (
  "${PROJECT_SOURCE_DIR}/version.rc.in"
  "${PROJECT_SOURCE_DIR}/version.rc"
)
add_definitions(-DMINIDEEN_X86)
include_directories(include)
add_custom_command(
  TARGET MiniDeen POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MiniDeen> "../Release_${VERSION}/${_DIR}/$<TARGET_FILE_NAME:MiniDeen>"
)
