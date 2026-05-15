cmake_minimum_required(VERSION 3.14)
project(AngryBirdsC LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Dependencies
include(FetchContent)

# Raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        master
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Chipmunk2D
FetchContent_Declare(
    chipmunk
    GIT_REPOSITORY https://github.com/slembcke/Chipmunk2D.git
    GIT_TAG        Chipmunk-7.0.3
)
set(BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(INSTALL_DEMOS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(chipmunk)

# cJSON
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.15
)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cjson)

# Application Sources
file(GLOB SOURCES "src/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${raylib_SOURCE_DIR}/src
    ${chipmunk_SOURCE_DIR}/include
    ${cjson_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    chipmunk
    cjson
)

# Configuration for Windows/Mingw
if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32)
endif()

# Copy assets to build directory
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})
