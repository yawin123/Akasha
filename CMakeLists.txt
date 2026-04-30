cmake_minimum_required(VERSION 3.16)

project(Akasha VERSION 2.0.0 LANGUAGES CXX)

# Options can be set from CMake or from Conan
option(BUILD_EXAMPLE "Build example application" OFF)
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_BENCHMARKS "Build benchmark suite" OFF)
option(AKASHA_BUILD_SINGLE_ARCHIVE "Build a bundled static archive with Akasha and its static dependencies" OFF)
option(AKASHA_THREAD_SAFE "Enable thread-safe operation with std::shared_mutex (required for multithreaded use)" OFF)

set(AKASHA_DEP_TARGETS "" CACHE STRING "Semicolon-separated static library targets linked privately into akasha")
set(AKASHA_BUNDLE_ARCHIVES "" CACHE STRING "Semicolon-separated additional static archive paths for bundling")

# Find Conan dependencies - add first to have priority
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
find_package(Boost REQUIRED)

add_library(akasha STATIC
    src/store_core.cpp
    src/store_data.cpp
    src/store_memory.cpp
    src/store_views.cpp
    src/batch.cpp
)

add_library(akasha::akasha ALIAS akasha)

target_include_directories(akasha
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(akasha PUBLIC cxx_std_23)

target_compile_definitions(akasha
    PUBLIC
        AKASHA_VERSION="${CMAKE_PROJECT_VERSION}"
        $<$<BOOL:${AKASHA_THREAD_SAFE}>:AKASHA_THREAD_SAFE>
)

target_link_libraries(akasha 
    PRIVATE 
    Boost::boost
)

if(AKASHA_DEP_TARGETS)
    target_link_libraries(akasha PRIVATE ${AKASHA_DEP_TARGETS})
endif()

if(BUILD_EXAMPLE)
    # Teaching examples demonstrating API capabilities
    
    # 1. Quick start: basic load, set, get, getorset, unload
    add_executable(akasha_quickstart examples/quickstart.cpp)
    target_link_libraries(akasha_quickstart PRIVATE akasha::akasha)
    
    # 2. Error handling: Status enum validation
    add_executable(akasha_error_handling examples/error_handling.cpp)
    target_link_libraries(akasha_error_handling PRIVATE akasha::akasha)
    
    # 3. Multiple datasets: loading multiple sources in one Store
    add_executable(akasha_multiple_datasets examples/multiple_datasets.cpp)
    target_link_libraries(akasha_multiple_datasets PRIVATE akasha::akasha)
    
    # 4. Navigation and introspection: DatasetView, has(), keys()
    add_executable(akasha_navigation examples/navigation.cpp)
    target_link_libraries(akasha_navigation PRIVATE akasha::akasha)
    
    # 5. Cleanup and lifecycle: clear() and compact() demonstrations
    add_executable(akasha_cleanup examples/cleanup.cpp)
    target_link_libraries(akasha_cleanup PRIVATE akasha::akasha)
    
    # 6. Nested data structures: trivially copyable structs
    add_executable(akasha_nested_data examples/nested_data.cpp)
    target_link_libraries(akasha_nested_data PRIVATE akasha::akasha)
    
    # 7. DatasetView: subtree navigation, introspection, and copying
    add_executable(akasha_datasetview examples/datasetview.cpp)
    target_link_libraries(akasha_datasetview PRIVATE akasha::akasha)
    
    # 8. Vectors: storing and retrieving std::vector<T> of scalar types
    add_executable(akasha_vectors examples/vectors.cpp)
    target_link_libraries(akasha_vectors PRIVATE akasha::akasha)
    
    # 9. Compaction: demonstrating defragmentation and space reclamation
    add_executable(akasha_compaction examples/compaction.cpp)
    target_link_libraries(akasha_compaction PRIVATE akasha::akasha)
    
    # 10. Performance Tuning: configuring file growth and memory allocation parameters
    add_executable(akasha_performance_tuning examples/performance_tuning.cpp)
    target_link_libraries(akasha_performance_tuning PRIVATE akasha::akasha)
    
    # 11. Serializable<T>: user-defined types with structured storage
    add_executable(akasha_serializable examples/serializable.cpp)
    target_link_libraries(akasha_serializable PRIVATE akasha::akasha)
    
    # 12. JSON roundtrip: JSON → Serializable<T> → Akasha roundtrip
    add_executable(akasha_json_roundtrip examples/json_roundtrip.cpp)
    target_include_directories(akasha_json_roundtrip PRIVATE "${CMAKE_BINARY_DIR}/nlohmann_json/include")
    target_link_libraries(akasha_json_roundtrip PRIVATE akasha::akasha)
    
    # Reset BUILD_EXAMPLE to OFF after processing so next build doesn't compile examples
    set(BUILD_EXAMPLE OFF CACHE BOOL "Build example application" FORCE)
endif()

if(BUILD_TESTS)
    # Test suite (header-only framework, no external dependencies)
    # All test files are included from basic_tests.cpp to ensure proper initialization
    add_executable(akasha_tests tests/basic_tests.cpp)
    target_link_libraries(akasha_tests PRIVATE akasha::akasha)
    target_include_directories(akasha_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/simple-test-framework)
endif()

if(BUILD_BENCHMARKS)
    # Benchmark suite using SBF (Simple Benchmark Framework)
    add_executable(akasha_benchmarks benchmarks/benchmark_main.cpp)
    target_link_libraries(akasha_benchmarks PRIVATE akasha::akasha)
    target_include_directories(akasha_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks ${CMAKE_BINARY_DIR}/simple-benchmark-framework/include ${CMAKE_BINARY_DIR}/simple-benchmark-framework/include/scf/include)
endif()

if(AKASHA_BUILD_SINGLE_ARCHIVE)
    if(NOT CMAKE_AR)
        message(FATAL_ERROR "AKASHA_BUILD_SINGLE_ARCHIVE requires an 'ar' tool available in CMAKE_AR")
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

# Configure akashaConfig.cmake from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/akashaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/akashaConfig.cmake"
    @ONLY
)

# Install akashaConfig.cmake
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/akashaConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/akasha
)
