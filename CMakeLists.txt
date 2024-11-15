cmake_minimum_required(VERSION 3.11.0)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(DalbaragiTools)

include(FetchContent)


set(DALTOOLS_ENABLE_TEST OFF CACHE BOOL "Enable testing")

set(source_dir ${PROJECT_SOURCE_DIR}/src)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# Define external libraries
# ----------------------------------------------------------------------------------

FetchContent_Declare(
    fetch_glob
    GIT_REPOSITORY https://github.com/p-ranav/glob
    GIT_TAG        d025092c0e1eb1a8b226d3a799fd32680d2fd13f
)

FetchContent_MakeAvailable(
    fetch_glob
)

add_subdirectory(${PROJECT_SOURCE_DIR}/extern/SungToolsCpp)

find_package(argparse CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb MODULE REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)


# Define project library
# ----------------------------------------------------------------------------------

add_library(libdaltools STATIC
    ${source_dir}/bundle/bundle.cpp
    ${source_dir}/bundle/repo.cpp
    ${source_dir}/common/byte_tool.cpp
    ${source_dir}/common/compression.cpp
    ${source_dir}/common/util.cpp
    ${source_dir}/dmd/exporter.cpp
    ${source_dir}/dmd/parser.cpp
    ${source_dir}/filesys/filesys.cpp
    ${source_dir}/img/backend/ktx.cpp
    ${source_dir}/img/backend/stb.cpp
    ${source_dir}/img/img.cpp
    ${source_dir}/json/parser.cpp
    ${source_dir}/scene/modifier_scene.cpp
    ${source_dir}/scene/modifier.cpp
    ${source_dir}/scene/struct.cpp
)
target_include_directories(libdaltools PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${Stb_INCLUDE_DIR}
)
target_compile_features(libdaltools PUBLIC cxx_std_17)
add_library(dalbaragi::daltools ALIAS libdaltools)


# Import external libraries
# ----------------------------------------------------------------------------------

if (ANDROID)
    target_link_libraries(libdaltools PRIVATE z)

else()
    find_package(ZLIB REQUIRED)
    target_link_libraries(libdaltools PRIVATE ZLIB::ZLIB)

endif()

target_link_libraries(libdaltools PUBLIC
    aklomp::base64
    glm::glm
    KTX::ktx
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    sungtools::general
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
)


# Add subdirectories
# ----------------------------------------------------------------------------------

add_subdirectory(./app)

if (DALTOOLS_ENABLE_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(./test)
endif()
