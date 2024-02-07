# Minimum required version of CMake, it is always good to go with the latest one you can get
# your hands on for the most recent features
# Here I specified an older version, to accomodate those who
# have an older version supported by their distro.
cmake_minimum_required(VERSION 3.15)

# Project name and version. This will set the CMake variable PROJECT_NAME
project(input-redirection VERSION 1.0)

# Define the source files for your project. At this moment we have only one
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Create an executable target named "input-redirection" from the source files
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

