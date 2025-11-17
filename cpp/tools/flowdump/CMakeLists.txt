# FlowDump - Multi-threaded flow generator tool

cmake_minimum_required(VERSION 3.15)

# Find threads package
find_package(Threads REQUIRED)

# FlowDump executable
add_executable(flowdump
    main.cpp
    enhanced_flow.cpp
    timestamp_chunker.cpp
    generator_worker.cpp
    flow_collector.cpp
    flow_formatter.cpp
)

# Link against flowgen library and threads
target_link_libraries(flowdump
    PRIVATE
        flowgen
        Threads::Threads
)

# Include directories
target_include_directories(flowdump
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/cpp/include
)

# Compile features
target_compile_features(flowdump PRIVATE cxx_std_17)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flowdump PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Install
install(TARGETS flowdump
        RUNTIME DESTINATION bin)
