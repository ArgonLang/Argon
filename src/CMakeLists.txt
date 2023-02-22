add_library(ArgonVM SHARED)
add_dependencies(ArgonVM Stratum)
target_link_libraries(ArgonVM Stratum)

if(WIN32)
    target_link_libraries(ArgonVM ws2_32)
endif()

if (NOT MSVC)
    target_compile_options(ArgonVM PRIVATE -fPIC)
endif ()

set_target_properties(ArgonVM PROPERTIES OUTPUT_NAME "argon")

# *** Main Executable ***

add_executable(${CMAKE_PROJECT_NAME} main.cc)
add_dependencies(${CMAKE_PROJECT_NAME} ArgonVM)
target_link_libraries(${CMAKE_PROJECT_NAME} ArgonVM)

add_subdirectory(lang)
add_subdirectory(vm)
