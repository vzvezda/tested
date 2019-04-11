# Define the header only library 'tested'
cmake_minimum_required(VERSION 3.1)

set(CUR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(tested INTERFACE)
target_include_directories(tested INTERFACE "${CUR_DIR}/include/")
target_sources(tested INTERFACE "$<BUILD_INTERFACE:${CUR_DIR}/include/tested.h>")
