cmake_minimum_required(VERSION 3.15)

# C++ Examples for FlowGen

add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE flowgen)

# Set C++ standard
set_target_properties(basic_usage PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Output directory - place in build root
set_target_properties(basic_usage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Copy libflowgen.so to build folder after building
add_custom_command(TARGET basic_usage POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:flowgen>
        ${CMAKE_BINARY_DIR}/$<TARGET_FILE_NAME:flowgen>
    COMMENT "Copying libflowgen.so to build directory"
)
