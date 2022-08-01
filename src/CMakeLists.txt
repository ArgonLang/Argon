cmake_minimum_required(VERSION 3.15)

add_library(Stratum SHARED arena.cpp memory.cpp memutil.cpp osmemory.cpp )

if(NOT MSVC)
target_compile_options(Stratum PRIVATE -fPIC)
endif()

set_target_properties(Stratum PROPERTIES OUTPUT_NAME "stratum")
