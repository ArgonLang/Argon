cmake_minimum_required(VERSION 3.7)

add_library(parser SHARED scanner/scanner.cc parser.cc)

add_executable(rlpl scanner/rlpl.cc)
add_dependencies(rlpl parser)
target_link_libraries(rlpl parser)