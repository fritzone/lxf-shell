# Minimum required version of CMake, it is always good to go with the latest one you can get
# your hands on for the most recent features
# Here I specified an older version, to accomodate those who
# have an older version supported by their distro.
cmake_minimum_required(VERSION 3.15)

# Project name and version. This will set the CMake variable PROJECT_NAME
project(lxf-shell VERSION 1.0)

# Set the C++ standard to C++20 to use the widest set of features which are supported 
# in most compilers at the time of writing but not fall in the pitfall that some
# compilers do not support some features of C++23
set(CMAKE_CXX_STANDARD 20)

# Define the source files for your project. At this moment we have only one
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# add the external dependencies subdirectory
add_subdirectory(ext)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

# Find LibExplain to link it with the application
find_library(LIB_EXPLAIN explain)
if(${LIB_EXPLAIN} STREQUAL "LIB_EXPLAIN-NOTFOUND")
    message(FATAL_ERROR "You need to install LibExplain to build this application")
else()
    message(STATUS "LibExplain found at: ${LIB_EXPLAIN}")
endif()

# The utils static library
add_subdirectory(utils)

# Specify the include directories for the project
target_include_directories(${PROJECT_NAME}
    PRIVATE
    utils
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/json/single_include
)

# Link our executable with libexplain and the utils library
target_link_libraries(${PROJECT_NAME} ${LIB_EXPLAIN} lxf-shell-utils)

# Tests are located in the tests subdirectory
add_subdirectory(tests)

# The plugins that we develop
add_subdirectory(plugins)
