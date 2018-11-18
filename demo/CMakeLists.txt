# build script for demo project
cmake_minimum_required(VERSION 3.9)

project(TestedDemo)

set(CUR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17) 

add_library(math_test STATIC math_test.cpp)
add_library(vector_test STATIC vector_test.cpp)
add_executable(test_runner test_runner.cpp ${CUR_DIR}/../include/tested.h)

set_property(TARGET math_test PROPERTY CXX_STANDARD 17)
set_property(TARGET vector_test PROPERTY CXX_STANDARD 17)
set_property(TARGET test_runner PROPERTY CXX_STANDARD 17)

target_link_libraries(test_runner math_test vector_test)

target_include_directories(math_test PUBLIC ${CUR_DIR}/../include)
target_include_directories(vector_test PUBLIC ${CUR_DIR}/../include)
target_include_directories(test_runner PUBLIC ${CUR_DIR}/../include)