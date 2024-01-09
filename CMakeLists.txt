cmake_minimum_required(VERSION 3.15)
project(Argon VERSION 0.4.2)

set(AR_MAJOR ${PROJECT_VERSION_MAJOR})
set(AR_MINOR ${PROJECT_VERSION_MINOR})
set(AR_PATCH ${PROJECT_VERSION_PATCH})
set(AR_RELEASE_LEVEL "alpha")

set(BASE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(BASE_OUTPUT_HEADERS ${BASE_OUTPUT_DIRECTORY}/include/argon)

if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    set(BASE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BASE_OUTPUT_DIRECTORY})

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    set(CMAKE_CXX_STANDARD 20)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

add_subdirectory(lib/stratum)
add_subdirectory(argon)
add_subdirectory(test EXCLUDE_FROM_ALL)

# *** Argon main executable ***

add_executable(${CMAKE_PROJECT_NAME} "${CMAKE_SOURCE_DIR}/argon/main.cc")
add_dependencies(${CMAKE_PROJECT_NAME} ArgonVM)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ArgonVM)

file(COPY arlib/ DESTINATION ${BASE_OUTPUT_DIRECTORY}/packages)

# *** Install ***

set(PROJECT_NAME_MAJOR_MINOR ${PROJECT_NAME}${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(PROJECT_NAME_OUTPUT_LIB "argon${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

# *** Export Argon headers file ***

file(GLOB_RECURSE HEADERS
        "${PROJECT_SOURCE_DIR}/argon/*.h"
        "${PROJECT_SOURCE_DIR}/argon/*.hpp")

file(MAKE_DIRECTORY ${BASE_OUTPUT_HEADERS})

foreach (header_file ${HEADERS})
    file(RELATIVE_PATH rel_header_file ${PROJECT_SOURCE_DIR}/argon ${header_file})
    get_filename_component(header_path "${rel_header_file}" DIRECTORY)
    file(MAKE_DIRECTORY ${BASE_OUTPUT_HEADERS}/${header_path})
    configure_file(${header_file} ${BASE_OUTPUT_HEADERS}/${rel_header_file} COPYONLY)
endforeach ()

install(TARGETS ${CMAKE_PROJECT_NAME} ArgonVM
        EXPORT ArgonTargets)

# Install headers
install(DIRECTORY ${BASE_OUTPUT_HEADERS} DESTINATION include)

# Install arlib
install(DIRECTORY ${PROJECT_SOURCE_DIR}/arlib/
        DESTINATION "lib/${PROJECT_NAME_OUTPUT_LIB}/packages")

install(EXPORT ArgonTargets
        FILE ArgonTargets.cmake
        NAMESPACE Argon::
        DESTINATION lib/cmake/${PROJECT_NAME_MAJOR_MINOR})

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ArgonConfig.cmake"
        INSTALL_DESTINATION "lib/cmake/${PROJECT_NAME_MAJOR_MINOR}")

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ArgonConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY AnyNewerVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ArgonConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ArgonConfigVersion.cmake
        DESTINATION lib/cmake/${PROJECT_NAME_MAJOR_MINOR})

# *** Export package ***

set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)

export(TARGETS ArgonVM
        NAMESPACE Argon::
        FILE "${CMAKE_CURRENT_BINARY_DIR}/ArgonTargets.cmake")

export(PACKAGE Argon)
