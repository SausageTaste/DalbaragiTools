cmake_minimum_required(VERSION 3.11.0)

include(FetchContent)

project(DalbaragiTools
    LANGUAGES CXX
)


set(source_dir ${PROJECT_SOURCE_DIR}/src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# Define external libraries
# ----------------------------------------------------------------------------------

FetchContent_Declare(
    fetch_glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        0.9.9.8
)

FetchContent_Declare(
    fetch_zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        v1.2.11
)

FetchContent_Declare(
    fetch_libhydrogen
    GIT_REPOSITORY https://github.com/jedisct1/libhydrogen
)

FetchContent_Declare(
    fetch_argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse
    GIT_TAG        v2.2
)

FetchContent_Declare(
    fetch_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG        v3.10.5
)

FetchContent_Declare(
    fetch_base64
    GIT_REPOSITORY https://github.com/aklomp/base64
    GIT_TAG        master
)

FetchContent_MakeAvailable(fetch_glm)
FetchContent_MakeAvailable(fetch_libhydrogen)
FetchContent_MakeAvailable(fetch_argparse)
FetchContent_MakeAvailable(fetch_json)
FetchContent_MakeAvailable(fetch_base64)


# Define project library
# ----------------------------------------------------------------------------------

add_library(libdaltools
    ${source_dir}/model_parser.cpp
    ${source_dir}/struct.cpp
    ${source_dir}/byte_tool.cpp
    ${source_dir}/modifier.cpp
    ${source_dir}/model_exporter.cpp
    ${source_dir}/crypto.cpp
    ${source_dir}/compression.cpp
    ${source_dir}/util.cpp
    ${source_dir}/json_parser.cpp
    ${source_dir}/modifier_scene.cpp
)

target_include_directories(libdaltools PUBLIC ./include
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(libdaltools PUBLIC cxx_std_17)

add_library(dalbaragi::daltools ALIAS libdaltools)


# Import external libraries
# ----------------------------------------------------------------------------------

if (ANDROID)
    target_link_libraries(libdaltools PRIVATE z)

else()
    FetchContent_MakeAvailable(fetch_zlib)
    target_link_libraries(libdaltools PRIVATE zlibstatic)
    target_include_directories(libdaltools PRIVATE ${fetch_zlib_SOURCE_DIR} ${fetch_zlib_BINARY_DIR})

endif()

target_link_libraries(libdaltools
    PUBLIC
        glm::glm
        hydrogen
        nlohmann_json::nlohmann_json
        base64
)


# Add subdirectories
# ----------------------------------------------------------------------------------

enable_testing()

add_subdirectory(./app)
add_subdirectory(./test)
