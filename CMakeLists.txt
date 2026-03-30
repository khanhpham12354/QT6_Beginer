cmake_minimum_required(VERSION 3.16)
project(qt_manual_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)

FetchContent_Declare(
        openapv_lib
        GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openapv
        GIT_TAG        main
)

FetchContent_GetProperties(openapv_lib)
if(NOT openapv_lib_POPULATED)
    FetchContent_Populate(openapv_lib)
    # 1. Fix lỗi thiếu file oapv.h ở project gốc
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/inc/oapv.h")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/inc")
        file(COPY "${openapv_lib_SOURCE_DIR}/inc/oapv.h" DESTINATION "${CMAKE_SOURCE_DIR}/inc")
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/pkgconfig/oapv.pc.in")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/pkgconfig")
        file(COPY "${openapv_lib_SOURCE_DIR}/pkgconfig/oapv.pc.in" DESTINATION "${CMAKE_SOURCE_DIR}/pkgconfig")
    endif()

    # add OpenAPV vào project
    add_subdirectory(${openapv_lib_SOURCE_DIR} ${openapv_lib_BINARY_DIR})
endif()

# Find Dependencies (Chỉ cần GLFW)
find_package(glfw3 REQUIRED)

add_executable(qt_manual_app
    QtApvTool/src/main.cpp
)

# Include directories
target_include_directories(qt_manual_app PRIVATE 
    "${CMAKE_SOURCE_DIR}/QtApvTool/inc"
    "${CMAKE_SOURCE_DIR}/inc"
)

# Link libraries
# Trên macOS, chúng ta cần link thêm Framework OpenGL có sẵn của hệ thống
find_library(OPENGL_LIBRARY OpenGL REQUIRED)

target_link_libraries(qt_manual_app
    PRIVATE
        ${OPENGL_LIBRARY}
        glfw
)
