cmake_minimum_required(VERSION 3.15)
project(flowgen VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(BUILD_EXAMPLES "Build C++ examples" ON)
option(BUILD_TOOLS "Build tools (flowdump, etc.)" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# FlowGen C++ library
set(FLOWGEN_SOURCES
    cpp/src/flow_record.cpp
    cpp/src/utils.cpp
    cpp/src/patterns.cpp
    cpp/src/generator.cpp
)

set(FLOWGEN_HEADERS
    cpp/include/flowgen/flow_record.hpp
    cpp/include/flowgen/utils.hpp
    cpp/include/flowgen/patterns.hpp
    cpp/include/flowgen/generator.hpp
)

# Create library
add_library(flowgen ${FLOWGEN_SOURCES} ${FLOWGEN_HEADERS})

target_include_directories(flowgen
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler flags
if(MSVC)
    target_compile_options(flowgen PRIVATE /W4)
else()
    target_compile_options(flowgen PRIVATE -Wall -Wextra -O3)
endif()

# Python bindings
if(BUILD_PYTHON_BINDINGS)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)

    # Always fetch pybind11 from GitHub to ensure Python 3.12 compatibility
    message(STATUS "Fetching pybind11 from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.1
    )
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(_flowgen_core cpp/bindings/python_bindings.cpp)
    target_link_libraries(_flowgen_core PRIVATE flowgen)

    # Set output directory for Python module
    set_target_properties(_flowgen_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/flowgen
    )
endif()

# Examples
if(BUILD_EXAMPLES)
    # add_subdirectory(examples/cpp EXCLUDE_FROM_ALL)
    add_subdirectory(examples/cpp)
endif()

# Tools
if(BUILD_TOOLS)
    add_subdirectory(cpp/tools/flowdump)
    add_subdirectory(cpp/tools/flowstats)
endif()

# Installation
install(TARGETS flowgen
    EXPORT flowgenTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY cpp/include/flowgen
    DESTINATION include
)

# Export targets
install(EXPORT flowgenTargets
    FILE flowgenTargets.cmake
    NAMESPACE flowgen::
    DESTINATION lib/cmake/flowgen
)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "flowgenConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/flowgenConfigVersion.cmake"
    DESTINATION lib/cmake/flowgen
)
