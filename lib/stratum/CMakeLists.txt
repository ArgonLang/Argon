cmake_minimum_required(VERSION 3.15)
project(Stratum VERSION 1.0.0)

set(PROJECT_NAME_MAJOR_MINOR ${PROJECT_NAME}${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

add_subdirectory(stratum)
add_subdirectory(test EXCLUDE_FROM_ALL)

# *** Install ***

install(TARGETS ${PROJECT_NAME}
        EXPORT StratumTargets
        DESTINATION lib)

install(DIRECTORY stratum/
        DESTINATION include/stratum
        FILES_MATCHING PATTERN "*.h")

install(EXPORT StratumTargets
        FILE StratumTargets.cmake
        DESTINATION lib/cmake/${PROJECT_NAME_MAJOR_MINOR})

include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/StratumConfig.cmake"
        INSTALL_DESTINATION "lib/cmake/${PROJECT_NAME_MAJOR_MINOR}"
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/StratumConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY AnyNewerVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/StratumConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/StratumConfigVersion.cmake
        DESTINATION lib/cmake/${PROJECT_NAME_MAJOR_MINOR})
        
# *** Export package ***

set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)

export(TARGETS ${PROJECT_NAME}
        FILE "${CMAKE_CURRENT_BINARY_DIR}/StratumTargets.cmake")

export(PACKAGE Stratum)

# *** Doxygen ***

option(BUILD_DOC "Build documentation" ON)
option(ENABLE_TESTS "Enable tests" OFF)

if (BUILD_DOC AND CMAKE_BUILD_TYPE MATCHES "^[Rr]elease")
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${PROJECT_SOURCE_DIR}/doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        message("Doxygen build started")

        add_custom_target(doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating documentation for Stratum"
                VERBATIM)
    else (DOXYGEN_FOUND)
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif ()
