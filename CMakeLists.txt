cmake_minimum_required(VERSION 3.14)
project(EduRTOS VERSION 1.0.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(include)

# Create library with kernel components
add_library(edurtos_kernel
    src/kernel/task.cpp
    src/kernel/scheduler.cpp
    src/kernel/kernel.cpp
    src/drivers/virtual_hardware.cpp
    src/util/console_visualizer.cpp
    src/util/console_dashboard.cpp
    src/util/test_tasks.cpp
    src/util/scheduler_logger.cpp
    src/util/fault_injector.cpp
    src/util/console_logger.cpp
)

# Example application
add_executable(edurtos_example examples/main.cpp)
target_link_libraries(edurtos_example edurtos_kernel)

# Test application
add_executable(edurtos_tests examples/test_tasks_main.cpp)
target_link_libraries(edurtos_tests edurtos_kernel)

# Installation
install(TARGETS edurtos_kernel DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS edurtos_example edurtos_tests DESTINATION bin)
