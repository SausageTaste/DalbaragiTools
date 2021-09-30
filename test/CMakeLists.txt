cmake_minimum_required(VERSION 3.4.1)


add_executable(dal_parser_test_1
    test_1.cpp
)
target_compile_features(dal_parser_test_1 PUBLIC cxx_std_17)
target_include_directories(dal_parser_test_1 PRIVATE .)
target_link_libraries(dal_parser_test_1 PRIVATE daltools)

add_test(test_1 dal_parser_test_1)
