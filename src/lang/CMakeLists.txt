cmake_minimum_required(VERSION 3.7)

set(SRC scanner/scanner.cc parser.cc compiler.cc transl_unit.cc basicblock.cc symtable.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})

add_executable(rlpl scanner/rlpl.cc)