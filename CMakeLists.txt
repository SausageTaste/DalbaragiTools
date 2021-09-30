cmake_minimum_required(VERSION 3.11.0)

include(FetchContent)

project(DalbaragiTools
    LANGUAGES CXX
)


set(source_dir ${PROJECT_SOURCE_DIR}/src)


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

FetchContent_MakeAvailable(fetch_glm)


# Define project library
# ----------------------------------------------------------------------------------

add_library(daltools
    ${source_dir}/model_parser.cpp
    ${source_dir}/struct.cpp
    ${source_dir}/byte_tool.cpp
    ${source_dir}/modifier.cpp
    ${source_dir}/model_exporter.cpp
)

target_include_directories(daltools PUBLIC ./include
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(daltools PUBLIC cxx_std_17)


# Import external libraries
# ----------------------------------------------------------------------------------

if (ANDROID)
    target_link_libraries(daltools PRIVATE z)

else()
    FetchContent_MakeAvailable(fetch_zlib)
    target_link_libraries(daltools PRIVATE zlibstatic)
    target_include_directories(daltools PRIVATE ${fetch_zlib_SOURCE_DIR} ${fetch_zlib_BINARY_DIR})

endif()

target_link_libraries(daltools
    PUBLIC
        glm::glm
)


# Add subdirectories
# ----------------------------------------------------------------------------------

enable_testing()

add_subdirectory(./app)
add_subdirectory(./test)
