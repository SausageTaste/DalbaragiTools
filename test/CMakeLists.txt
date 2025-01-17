set(gtest_libs GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_executable            (daltest_dmd test_dmd.cpp)
target_compile_features   (daltest_dmd PUBLIC  cxx_std_17)
target_include_directories(daltest_dmd PRIVATE .)
target_link_libraries     (daltest_dmd PRIVATE dalbaragi::dalbaragi_tools)
add_test                  (daltest_dmd daltest_dmd)

add_executable            (daltest_json test_json.cpp)
target_compile_features   (daltest_json PUBLIC  cxx_std_17)
target_include_directories(daltest_json PRIVATE .)
target_link_libraries     (daltest_json PRIVATE ${gtest_libs} dalbaragi::dalbaragi_tools)
add_test                  (daltest_json daltest_json)

add_executable(daltest_crypto test_crypto.cpp)
add_test(daltest_crypto daltest_crypto)
target_link_libraries(daltest_crypto ${gtest_libs} dalbaragi::dalbaragi_tools)

add_executable(daltest_filesys test_filesys.cpp)
add_test(daltest_filesys daltest_filesys)
target_link_libraries(daltest_filesys ${gtest_libs} dalbaragi::dalbaragi_tools)

add_executable(daltest_zip test_zip.cpp)
add_test(daltest_zip daltest_zip)
target_link_libraries(daltest_zip ${gtest_libs} dalbaragi::dalbaragi_tools)

add_executable(daltest_img test_img.cpp)
add_test(daltest_img daltest_img)
target_link_libraries(daltest_img ${gtest_libs} dalbaragi::dalbaragi_tools)
