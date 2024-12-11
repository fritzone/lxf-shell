cmake_minimum_required(VERSION 3.15)

#
# The tests will be a different project, handled automatically by CMake
#
project(colored_prompt VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)

# Define the source files for the project. At this moment we have only one
# source file in the current directory: plugin.cpp
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/colored_prompt.cpp
    ${CMAKE_SOURCE_DIR}/plugins/plugin.h
)

# Adding the executable for the unit tests
add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SOURCES})

# specify the utils library
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    lxf-shell-utils
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Where to look for the project specific header files
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/plugins
)
