cmake_minimum_required(VERSION 3.7)

set(SRC scanner/scanner.cc
        scanner/token.cc
        parser/parser.cc
        parser/nodes.cc
        compiler/compiler.cc
        compiler/translation_unit.cc
        compiler/basicblock.cc
        compiler/symtable.cc
        compiler_wrapper.cc)

list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})

#add_executable(rlpl scanner/rlpl.cc scanner/scanner.cc)