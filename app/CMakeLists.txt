add_executable(daltools
    ops/work_bundle.cpp
    ops/work_compile.cpp
    ops/work_key.cpp
    ops/work_keygen.cpp
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
