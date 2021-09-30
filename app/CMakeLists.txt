cmake_minimum_required(VERSION 3.4.1)


add_executable(dal_model_tool
    main.cpp
)
target_include_directories(dal_model_tool PUBLIC .)
target_compile_features(dal_model_tool PUBLIC cxx_std_17)


######## Import libraries ########

target_link_libraries(dal_model_tool
    PRIVATE
        dalbaragi::daltools
)
