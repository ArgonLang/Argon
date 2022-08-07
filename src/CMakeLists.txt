add_library(ArgonVM SHARED)
add_dependencies(ArgonVM Stratum)
target_link_libraries(ArgonVM Stratum)

if (NOT MSVC)
    target_compile_options(ArgonVM PRIVATE -fPIC)
endif ()

set_target_properties(ArgonVM PROPERTIES OUTPUT_NAME "argon")

# *** Main Executable ***

add_executable(Argon main.cc)
add_dependencies(Argon ArgonVM)
target_link_libraries(Argon ArgonVM)

add_subdirectory(lang)
add_subdirectory(vm)
