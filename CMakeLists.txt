cmake_minimum_required(VERSION 3.16)

project(Akasha VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_EXAMPLE "Build example application" ON)
option(AKASHA_BUILD_SINGLE_ARCHIVE "Build a bundled static archive with Akasha and its static dependencies" OFF)

set(AKASHA_DEP_TARGETS "" CACHE STRING "Semicolon-separated static library targets linked privately into akasha")
set(AKASHA_BUNDLE_ARCHIVES "" CACHE STRING "Semicolon-separated additional static archive paths for bundling")

# Buscar dependencias de Conan
find_package(Boost REQUIRED)
find_package(flatbuffers REQUIRED)

add_library(akasha STATIC
    src/akasha.cpp
)

add_library(akasha::akasha ALIAS akasha)

target_include_directories(akasha
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(akasha PUBLIC cxx_std_23)

target_link_libraries(akasha 
    PRIVATE 
        Boost::boost
        flatbuffers::flatbuffers
)

if(AKASHA_DEP_TARGETS)
    target_link_libraries(akasha PRIVATE ${AKASHA_DEP_TARGETS})
endif()

if(BUILD_EXAMPLE)
    add_executable(akasha_example apps/example.cpp)
    target_link_libraries(akasha_example PRIVATE akasha::akasha)
endif()

if(AKASHA_BUILD_SINGLE_ARCHIVE)
    if(NOT CMAKE_AR)
        message(FATAL_ERROR "AKASHA_BUILD_SINGLE_ARCHIVE requiere una herramienta 'ar' disponible en CMAKE_AR")
    endif()

    set(_akasha_bundle_inputs "$<TARGET_FILE:akasha>")
    foreach(dep_target IN LISTS AKASHA_DEP_TARGETS)
        list(APPEND _akasha_bundle_inputs "$<TARGET_FILE:${dep_target}>")
    endforeach()
    list(APPEND _akasha_bundle_inputs ${AKASHA_BUNDLE_ARCHIVES})

    string(JOIN ";" _akasha_bundle_inputs_arg ${_akasha_bundle_inputs})
    set(AKASHA_BUNDLE_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}akasha_bundle${CMAKE_STATIC_LIBRARY_SUFFIX}")

    add_custom_command(
        OUTPUT "${AKASHA_BUNDLE_OUTPUT}"
        COMMAND ${CMAKE_COMMAND}
            "-DAKASHA_AR=${CMAKE_AR}"
            "-DAKASHA_RANLIB=${CMAKE_RANLIB}"
            "-DAKASHA_OUTPUT=${AKASHA_BUNDLE_OUTPUT}"
            "-DAKASHA_INPUTS=${_akasha_bundle_inputs_arg}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleStaticArchive.cmake"
        DEPENDS akasha ${AKASHA_DEP_TARGETS}
        VERBATIM
    )

    add_custom_target(akasha_bundle DEPENDS "${AKASHA_BUNDLE_OUTPUT}")
endif()

include(GNUInstallDirs)

install(TARGETS akasha
    EXPORT akashaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES include/akasha.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT akashaTargets
    FILE akashaTargets.cmake
    NAMESPACE akasha::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/akasha
)
