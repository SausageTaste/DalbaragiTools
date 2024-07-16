cmake_minimum_required(VERSION 3.11.0)

include(FetchContent)

project(DalbaragiTools)


set(source_dir ${PROJECT_SOURCE_DIR}/src)
set(DALTOOLS_ENABLE_TEST OFF CACHE BOOL "Enable testing")
set(BUILD_SHARED_LIBS OFF)
set(FETCHCONTENT_QUIET FALSE)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# Define external libraries
# ----------------------------------------------------------------------------------

FetchContent_Declare(
    fetch_glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        1.0.1
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        v1.3.1
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_libhydrogen
    GIT_REPOSITORY https://github.com/jedisct1/libhydrogen
    GIT_TAG        c18e510d23c7539629e306d47925a35327eb1ebf
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse
    GIT_TAG        v3.0
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG        v3.11.3
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.14.1
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_base64
    GIT_REPOSITORY https://github.com/aklomp/base64
    GIT_TAG        b20a31a997e0b48274fa09e58b65ee9202531e4f
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.14.0
    GIT_PROGRESS   TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    fetch_brotli
    GIT_REPOSITORY https://github.com/google/brotli
    GIT_TAG        v1.1.0
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fetch_ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
    GIT_TAG        v4.3.2
    GIT_PROGRESS   TRUE
)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)


FetchContent_MakeAvailable(
    fetch_glm
    fetch_libhydrogen
    fetch_argparse
    fetch_json
    fetch_spdlog
    fetch_base64
    fetch_brotli
    fetch_ktx
)


# Define project library
# ----------------------------------------------------------------------------------

add_library(libdaltools STATIC
    ${source_dir}/common/byte_tool.cpp
    ${source_dir}/common/compression.cpp
    ${source_dir}/common/crypto.cpp
    ${source_dir}/common/util.cpp
    ${source_dir}/dmd/exporter.cpp
    ${source_dir}/dmd/parser.cpp
    ${source_dir}/json/parser.cpp
    ${source_dir}/scene/modifier_scene.cpp
    ${source_dir}/scene/modifier.cpp
    ${source_dir}/scene/struct.cpp
)
target_include_directories(libdaltools PUBLIC
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
    target_include_directories(libdaltools PRIVATE
        ${fetch_zlib_SOURCE_DIR}
        ${fetch_zlib_BINARY_DIR}
    )

endif()

target_link_libraries(libdaltools PUBLIC
    glm::glm
    hydrogen
    nlohmann_json::nlohmann_json
    base64
    spdlog::spdlog
    brotlidec
    brotlienc
    ktx
)


# Add subdirectories
# ----------------------------------------------------------------------------------

add_subdirectory(./app)


if (DALTOOLS_ENABLE_TEST)
    if (NOT TARGET gtest)
        FetchContent_MakeAvailable(fetch_googletest)
    endif()

    enable_testing()
    add_subdirectory(./test)
endif()
