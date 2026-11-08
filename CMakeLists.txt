cmake_minimum_required(VERSION 3.24)
project(Pure VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(library)
add_subdirectory(examples)
