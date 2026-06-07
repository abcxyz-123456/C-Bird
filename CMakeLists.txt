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

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Set the output name of the executable to bird-release
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "bird-release")

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${raylib_SOURCE_DIR}/src
    ${cjson_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    cjson
)

# Configuration for Windows/Mingw
if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 -static)
endif()

# Copy assets to build directory
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

# Custom target to package the release executable and assets into a zip archive
add_custom_target(package_release
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/bird-release.zip
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/bird-release/README.md
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bird-release
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bird-release.exe ${CMAKE_SOURCE_DIR}/bird-release/bird-release.exe
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/bird-release/assets
    COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_SOURCE_DIR}/bird-release.zip --format=zip ${CMAKE_SOURCE_DIR}/bird-release
)
