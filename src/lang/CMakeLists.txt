cmake_minimum_required(VERSION 3.7)

add_library(parser SHARED scanner/scanner.cc parser.cc)
add_library(compiler SHARED compiler.cc basicblock.cc symbol_table.cc)
add_dependencies(compiler parser)
target_link_libraries(compiler parser)

add_executable(rlpl scanner/rlpl.cc)
add_dependencies(rlpl parser)
target_link_libraries(rlpl parser)