cmake_minimum_required(VERSION 3.4.1)


add_executable(daltools
    main.cpp
)
target_include_directories(daltools PUBLIC .)
target_compile_features(daltools PUBLIC cxx_std_17)


######## Import libraries ########

target_link_libraries(daltools
    PRIVATE
        dalbaragi::daltools
        argparse::argparse
)
