cmake_minimum_required(VERSION 3.4.1)


add_executable(dal_parser_test_1 test_1.cpp)
target_compile_features   (dal_parser_test_1 PUBLIC  cxx_std_17)
target_include_directories(dal_parser_test_1 PRIVATE .)
target_link_libraries     (dal_parser_test_1 PRIVATE dalbaragi::daltools)
add_test(test_1 dal_parser_test_1)

add_executable(dal_parser_test_2 test_2.cpp)
target_compile_features   (dal_parser_test_2 PUBLIC  cxx_std_17)
target_include_directories(dal_parser_test_2 PRIVATE .)
target_link_libraries     (dal_parser_test_2 PRIVATE dalbaragi::daltools hydrogen)
add_test(test_1 dal_parser_test_2)

add_executable(dal_parser_test_json test_json.cpp)
target_compile_features   (dal_parser_test_json PUBLIC  cxx_std_17)
target_include_directories(dal_parser_test_json PRIVATE .)
target_link_libraries     (dal_parser_test_json PRIVATE dalbaragi::daltools)
add_test(test_1 dal_parser_test_json)
