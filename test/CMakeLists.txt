cmake_minimum_required(VERSION 3.4.1)


add_executable(dal_parser_test_1 test_1.cpp)
target_compile_features   (dal_parser_test_1 PUBLIC  cxx_std_17)
target_include_directories(dal_parser_test_1 PRIVATE .)
target_link_libraries     (dal_parser_test_1 PRIVATE dalbaragi::daltools)
add_test(test_1 dal_parser_test_1)

add_executable            (daltest_keygen test_keygen.cpp)
target_compile_features   (daltest_keygen PUBLIC  cxx_std_17)
target_include_directories(daltest_keygen PRIVATE .)
target_link_libraries     (daltest_keygen PRIVATE dalbaragi::daltools hydrogen)
add_test                  (daltest_keygen daltest_keygen)

add_executable            (daltest_json test_json.cpp)
target_compile_features   (daltest_json PUBLIC  cxx_std_17)
target_include_directories(daltest_json PRIVATE .)
target_link_libraries     (daltest_json PRIVATE dalbaragi::daltools hydrogen)
add_test                  (daltest_json daltest_json)
