cmake_minimum_required(VERSION 3.15)

#
# The tests will be a different project, handled automatically by CMake
#
project(lxf-shell-tests VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)

# Define the source files for the project. At this moment we have only one
# source file in the current directory: tests.cpp and the file we want to test
# from the parent directory: utils.cpp
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests.cpp
    ${CMAKE_SOURCE_DIR}/utils.cpp
)

# Adding the executable for the unit tests
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

# Where to look for the project specific header files
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

# Unit testing support for Catch2 starts here
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
