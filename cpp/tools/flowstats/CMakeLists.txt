# FlowStats tool
cmake_minimum_required(VERSION 3.15)

# Find threads
find_package(Threads REQUIRED)

# Source files
set(FLOWSTATS_SOURCES
    main.cpp
    core/progress_tracker.cpp
    utils/enhanced_flow.cpp
)

# Create executable
add_executable(flowstats ${FLOWSTATS_SOURCES})

# Link against flowgen library and threads
target_link_libraries(flowstats
    PRIVATE
        flowgen
        Threads::Threads
)

# Include directories
target_include_directories(flowstats
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/cpp/include
)

# C++17 standard
target_compile_features(flowstats PRIVATE cxx_std_17)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flowstats PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# Install
install(TARGETS flowstats
        RUNTIME DESTINATION bin)
