cmake_minimum_required(VERSION 3.15)

#
# The utils will be a different project, handled automatically by CMake
#
project(lxf-shell-utils VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)

# Define the source files for the project. At this moment we have only one
# source file in the current directory: utils.cpp
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sys_tools.cpp
)

# Adding the STATIC library for utils
add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SOURCES})
