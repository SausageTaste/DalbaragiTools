cmake_minimum_required(VERSION 3.11.0)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(Dalbaragi
    VERSION 0.1.0
    LANGUAGES CXX
)


set(DALTOOLS_ENABLE_TEST OFF CACHE BOOL "Enable testing")

set(source_dir ${PROJECT_SOURCE_DIR}/src)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# Define external libraries
# ----------------------------------------------------------------------------------

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

add_library(dalbaragi_tools STATIC
    ${source_dir}/bundle/bundle.cpp
    ${source_dir}/bundle/repo.cpp
    ${source_dir}/common/byte_tool.cpp
    ${source_dir}/common/compression.cpp
    ${source_dir}/common/util.cpp
    ${source_dir}/dmd/exporter.cpp
    ${source_dir}/dmd/parser.cpp
    ${source_dir}/filesys/filesys.cpp
    ${source_dir}/filesys/res_mgr.cpp
    ${source_dir}/img/backend/ktx.cpp
    ${source_dir}/img/backend/stb.cpp
    ${source_dir}/img/img.cpp
    ${source_dir}/json/parser.cpp
    ${source_dir}/scene/modifier_scene.cpp
    ${source_dir}/scene/modifier.cpp
    ${source_dir}/scene/struct.cpp
)
target_include_directories(dalbaragi_tools PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${Stb_INCLUDE_DIR}
)
target_compile_features(dalbaragi_tools PUBLIC cxx_std_17)
add_library(dalbaragi::dalbaragi_tools ALIAS dalbaragi_tools)


# Import external libraries
# ----------------------------------------------------------------------------------

if (ANDROID)
    target_link_libraries(dalbaragi_tools PRIVATE z)

else()
    find_package(ZLIB REQUIRED)
    target_link_libraries(dalbaragi_tools PRIVATE ZLIB::ZLIB)

endif()

target_link_libraries(dalbaragi_tools PUBLIC
    aklomp::base64
    glm::glm
    KTX::ktx
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    sungtools::sungtools_basic
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
)


# Install
# ----------------------------------------------------------------------------------


# Add subdirectories
# ----------------------------------------------------------------------------------

add_subdirectory(./app)

if (DALTOOLS_ENABLE_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(./test)
endif()
