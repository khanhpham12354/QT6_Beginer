cmake_minimum_required(VERSION 3.16)
project(qt_manual_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Dependencies
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(qt_manual_app
    QtApvTool/src/main.cpp
)

# Include directories
target_include_directories(qt_manual_app PRIVATE 
    "${CMAKE_SOURCE_DIR}/inc"
)

# Link libraries
target_link_libraries(qt_manual_app
    PRIVATE
        GLEW::GLEW
        glfw
)
