cmake_minimum_required(VERSION 3.4.1)


add_executable            (daltest_dmd test_dmd.cpp)
target_compile_features   (daltest_dmd PUBLIC  cxx_std_17)
target_include_directories(daltest_dmd PRIVATE .)
target_link_libraries     (daltest_dmd PRIVATE dalbaragi::daltools)
add_test                  (daltest_dmd daltest_dmd)

add_executable            (daltest_keygen test_keygen.cpp)
target_compile_features   (daltest_keygen PUBLIC  cxx_std_17)
target_include_directories(daltest_keygen PRIVATE .)
target_link_libraries     (daltest_keygen PRIVATE dalbaragi::daltools hydrogen)
add_test                  (daltest_keygen daltest_keygen)

add_executable            (daltest_json test_json.cpp)
target_compile_features   (daltest_json PUBLIC  cxx_std_17)
target_include_directories(daltest_json PRIVATE .)
target_link_libraries     (daltest_json PRIVATE GTest::gtest_main dalbaragi::daltools hydrogen)
add_test                  (daltest_json daltest_json)
